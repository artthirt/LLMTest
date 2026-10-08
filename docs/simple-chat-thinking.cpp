#include "llama.h"
#include "chat.h"

#include <clocale>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// Fill the batch with tokens at consecutive positions starting from pos_0.
// Output logits only for the last token.
static void batch_set_tokens(
        llama_batch_ext * batch,
        const llama_token * tokens,
        int32_t n_tokens,
        llama_pos pos_0) {

    llama_batch_ext_clear(batch);

    for (int32_t i = 0; i < n_tokens; ++i) {
        const int32_t idx = llama_batch_ext_add_token(batch, 0, tokens[i]);
        const llama_pos pos = pos_0 + i;

        llama_batch_ext_set_pos(batch, idx, &pos);
    }

    llama_batch_ext_set_output_logits(batch, n_tokens - 1, true);
}

static void print_usage(const char * argv0) {
    printf("\nusage:\n");
    printf("    %s -m model.gguf [-c context_size] [-ngl n_gpu_layers]\n",
           argv0);
    printf("\n");
    printf("commands:\n");
    printf("    /think       enable thinking\n");
    printf("    /nothink     disable thinking\n");
    printf("    /exit        exit\n");
    printf("\n");
}

int main(int argc, char ** argv) {
    std::setlocale(LC_NUMERIC, "C");

    std::string model_path;

    int ngl = 99;
    int n_ctx = 2048;

    // ---------------------------------------------------------------------
    // Command line
    // ---------------------------------------------------------------------

    for (int i = 1; i < argc; ++i) {
        try {
            if (strcmp(argv[i], "-m") == 0) {
                if (i + 1 < argc) {
                    model_path = argv[++i];
                } else {
                    print_usage(argv[0]);
                    return 1;
                }
            } else if (strcmp(argv[i], "-c") == 0) {
                if (i + 1 < argc) {
                    n_ctx = std::stoi(argv[++i]);
                } else {
                    print_usage(argv[0]);
                    return 1;
                }
            } else if (strcmp(argv[i], "-ngl") == 0) {
                if (i + 1 < argc) {
                    ngl = std::stoi(argv[++i]);
                } else {
                    print_usage(argv[0]);
                    return 1;
                }
            } else {
                print_usage(argv[0]);
                return 1;
            }
        } catch (const std::exception & e) {
            fprintf(stderr, "error: %s\n", e.what());
            print_usage(argv[0]);
            return 1;
        }
    }

    if (model_path.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    // ---------------------------------------------------------------------
    // llama.cpp initialization
    // ---------------------------------------------------------------------

    llama_log_set(
        [](enum ggml_log_level level, const char * text, void *) {
            if (level >= GGML_LOG_LEVEL_ERROR) {
                fprintf(stderr, "%s", text);
            }
        },
        nullptr
    );

    llama_backend_init();

    // ---------------------------------------------------------------------
    // Model
    // ---------------------------------------------------------------------

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = ngl;

    llama_model * model =
        llama_model_load_from_file(model_path.c_str(), model_params);

    if (!model) {
        fprintf(stderr,
                "%s: error: unable to load model\n",
                __func__);
        return 1;
    }

    const llama_vocab * vocab = llama_model_get_vocab(model);

    // ---------------------------------------------------------------------
    // Context
    // ---------------------------------------------------------------------

    llama_context_params ctx_params = llama_context_default_params();

    ctx_params.n_ctx = n_ctx;
    ctx_params.n_batch = n_ctx;

    llama_context * ctx =
        llama_init_from_model(model, ctx_params);

    if (!ctx) {
        fprintf(stderr,
                "%s: error: failed to create llama_context\n",
                __func__);

        llama_model_free(model);
        return 1;
    }

    // ---------------------------------------------------------------------
    // Sampler
    // ---------------------------------------------------------------------

    llama_sampler * smpl =
        llama_sampler_chain_init(
            llama_sampler_chain_default_params());

    llama_sampler_chain_add(
        smpl,
        llama_sampler_init_min_p(0.05f, 1));

    llama_sampler_chain_add(
        smpl,
        llama_sampler_init_temp(0.8f));

    llama_sampler_chain_add(
        smpl,
        llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // ---------------------------------------------------------------------
    // Chat template
    // ---------------------------------------------------------------------

    //
    // This is the important difference from the original simple-chat.
    //
    // common_chat_templates uses the current Jinja template from the
    // model metadata and gives us access to:
    //
    //     inputs.enable_thinking
    //     inputs.chat_template_kwargs
    //
    // Both are supported by the current common/chat API.
    //

    common_chat_templates_ptr chat_templates =
        common_chat_templates_init(
            model,
            ""
        );

    if (!chat_templates) {
        fprintf(stderr,
                "failed to initialize chat templates\n");

        llama_sampler_free(smpl);
        llama_free(ctx);
        llama_model_free(model);

        return 1;
    }

    if (!common_chat_templates_support_enable_thinking(
            chat_templates.get())) {

        fprintf(stderr,
                "warning: chat template does not appear to "
                "support enable_thinking\n");
    }

    // ---------------------------------------------------------------------
    // Batch
    // ---------------------------------------------------------------------

    llama_batch_ext * batch =
        llama_batch_ext_init(ctx);

    // ---------------------------------------------------------------------
    // Conversation
    // ---------------------------------------------------------------------

    std::vector<common_chat_msg> messages;

    // ---------------------------------------------------------------------
    // Thinking state
    // ---------------------------------------------------------------------

    bool enable_thinking = true;

    // ---------------------------------------------------------------------
    // Generation
    // ---------------------------------------------------------------------

    auto generate = [&](const std::string & prompt) {

        std::string response;

        const bool is_first =
            llama_memory_seq_pos_max(
                llama_get_memory(ctx), 0) == -1;

        // Tokenize prompt.
        const int n_prompt_tokens =
            -llama_tokenize(
                vocab,
                prompt.c_str(),
                prompt.size(),
                nullptr,
                0,
                is_first,
                true
            );

        std::vector<llama_token> prompt_tokens(
            n_prompt_tokens
        );

        if (llama_tokenize(
                vocab,
                prompt.c_str(),
                prompt.size(),
                prompt_tokens.data(),
                prompt_tokens.size(),
                is_first,
                true) < 0) {

            GGML_ABORT("failed to tokenize prompt\n");
        }

        const llama_token * tokens =
            prompt_tokens.data();

        int n_tokens =
            static_cast<int>(prompt_tokens.size());

        while (true) {

            int context_size = llama_n_ctx(ctx);

            int context_used =
                llama_memory_seq_pos_max(
                    llama_get_memory(ctx), 0) + 1;

            if (context_used + n_tokens > context_size) {
                printf("\033[0m\n");

                fprintf(stderr,
                        "context size exceeded\n");

                exit(0);
            }

            batch_set_tokens(
                batch,
                tokens,
                n_tokens,
                context_used
            );

            int ret =
                llama_process(
                    ctx,
                    LLAMA_PROCESS_TYPE_DECODE,
                    batch
                );

            if (ret != 0) {
                GGML_ABORT(
                    "failed to decode, ret = %d\n",
                    ret
                );
            }

            llama_token new_token =
                llama_sampler_sample(
                    smpl,
                    ctx,
                    -1
                );

            if (llama_vocab_is_eog(
                    vocab,
                    new_token)) {

                break;
            }

            char buf[256];

            int n =
                llama_token_to_piece(
                    vocab,
                    new_token,
                    buf,
                    sizeof(buf),
                    0,
                    true
                );

            if (n < 0) {
                GGML_ABORT(
                    "failed to convert token to piece\n"
                );
            }

            std::string piece(buf, n);

            printf("%s", piece.c_str());
            fflush(stdout);

            response += piece;

            // Next iteration processes the sampled token.
            tokens = &new_token;
            n_tokens = 1;
        }

        return response;
    };

    // ---------------------------------------------------------------------
    // Previous formatted prompt length
    // ---------------------------------------------------------------------

    std::string previous_formatted;

    // ---------------------------------------------------------------------
    // Chat loop
    // ---------------------------------------------------------------------

    while (true) {

        printf("\033[32m> \033[0m");
        fflush(stdout);

        std::string user;

        if (!std::getline(std::cin, user)) {
            break;
        }

        if (user.empty()) {
            break;
        }

        // -------------------------------------------------------------
        // Commands
        // -------------------------------------------------------------

        if (user == "/exit") {
            break;
        }

        if (user == "/think") {
            enable_thinking = true;

            printf(
                "\033[36mThinking enabled\033[0m\n"
            );

            continue;
        }

        if (user == "/nothink") {
            enable_thinking = false;

            printf(
                "\033[36mThinking disabled\033[0m\n"
            );

            continue;
        }

        // -------------------------------------------------------------
        // Add user message
        // -------------------------------------------------------------

        common_chat_msg msg;

        msg.role = "user";
        msg.content = user;

        messages.push_back(std::move(msg));

        // -------------------------------------------------------------
        // Format conversation
        // -------------------------------------------------------------

        common_chat_templates_inputs inputs;

        inputs.messages = messages;

        inputs.add_generation_prompt = true;

        inputs.use_jinja = true;

        //
        // This is the key:
        //
        //     true  -> template receives enable_thinking=true
        //     false -> template receives enable_thinking=false
        //
        inputs.enable_thinking = enable_thinking;

        common_chat_params params;

        try {
            params =
                common_chat_templates_apply(
                    chat_templates.get(),
                    inputs
                );
        } catch (const std::exception & e) {

            fprintf(stderr,
                    "failed to apply chat template: %s\n",
                    e.what());

            return 1;
        }

        const std::string & formatted =
            params.prompt;

        // -------------------------------------------------------------
        // Only evaluate the new part of the prompt.
        //
        // This is equivalent to the prev_len logic in simple-chat,
        // but std::string makes it easier to deal with the Jinja
        // template.
        // -------------------------------------------------------------

        size_t previous_size =
            previous_formatted.size();

        if (previous_size > formatted.size() ||
            formatted.compare(
                0,
                previous_size,
                previous_formatted) != 0) {

            //
            // The formatted prompt changed in a way that is not simply
            // an extension of the previous prompt.
            //
            // For example, changing thinking mode may change the
            // assistant generation prefix.
            //
            // In this simple example we restart the conversation
            // context in that case.
            //

            llama_memory_clear(
                llama_get_memory(ctx),
                true
            );

            previous_size = 0;
        }

        std::string prompt =
            formatted.substr(previous_size);

        // -------------------------------------------------------------
        // Generate
        // -------------------------------------------------------------

        printf("\033[33m");

        std::string response =
            generate(prompt);

        printf("\n\033[0m");

        // -------------------------------------------------------------
        // Add assistant response
        // -------------------------------------------------------------

        common_chat_msg assistant;

        assistant.role = "assistant";
        assistant.content = response;

        messages.push_back(std::move(assistant));

        // -------------------------------------------------------------
        // Generate the complete formatted conversation WITHOUT
        // the generation prompt. This becomes our prefix next time.
        // -------------------------------------------------------------

        common_chat_templates_inputs history_inputs;

        history_inputs.messages = messages;

        history_inputs.add_generation_prompt = false;

        history_inputs.use_jinja = true;

        history_inputs.enable_thinking = enable_thinking;

        try {
            previous_formatted =
                common_chat_templates_apply(
                    chat_templates.get(),
                    history_inputs
                ).prompt;
        } catch (const std::exception & e) {

            fprintf(stderr,
                    "failed to format conversation history: %s\n",
                    e.what());

            return 1;
        }
    }

    // ---------------------------------------------------------------------
    // Cleanup
    // ---------------------------------------------------------------------

    llama_batch_ext_free(batch);

    llama_sampler_free(smpl);

    llama_free(ctx);

    llama_model_free(model);

    return 0;
}