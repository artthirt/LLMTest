#include "llamaworker.h"
#include "chat.h"

#include <QElapsedTimer>

//////////////////////////

void batch_set_tokens(llama_batch_ext *batch, const llama_token *tokens, int n_tokens,
                      llama_pos pos_0)
{
    llama_batch_ext_clear(batch);
    for(int i = 0; i < n_tokens; ++i){
        int idx = llama_batch_ext_add_token(batch, 0, tokens[i]);
        llama_pos pos = pos_0 + i;
        llama_batch_ext_set_pos(batch, idx, &pos);
    }
    llama_batch_ext_set_output_logits(batch, n_tokens - 1, true);
}

struct Utf8Hold{
    std::vector<char> tail;

    static size_t completeLen(const char *b, size_t n){
        for(size_t k = 1; k <= 4 && k <= n; ++k){
            auto c = (uint8_t)b[n - k];
            if((c & 0xC0) == 0x80) continue;
            int len = c < 0x80 ? 1: (c & 0xE0) == 0xC0? 2 : (c & 0xF0) == 0xE0? 3 : 4;
            return (int)k < len? n - k : n;
        }
        return n;
    }

    std::string append(const char *b, int n){
        tail.insert(tail.end(), b, b + n);
        size_t len = completeLen(tail.data(), tail.size());
        std::string out(tail.begin(), tail.begin() + len);
        tail.erase(tail.begin(), tail.begin() + len);
        return out;
    }

    std::string flush() {
        auto out = std::string(tail.begin(), tail.end());
        tail.clear();
        return out;
    }
};

//////////////////////////

LlamaWorker::LlamaWorker()
    : QThread()
{
    llama_backend_init();

    moveToThread(this);
    start();
}

LlamaWorker::~LlamaWorker()
{
    stopGenerate();
    quit();
    wait();
}

void LlamaWorker::loadModel(const QString &gguf, const Params &params)
{
    releaseModel();

    mParams = params;

    mIsError = false;
    mIsLoading = true;

    mUseThinking = params.enable_thinking;

    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = params.gpu_layers;
    mp.progress_callback_user_data = this;
    mp.progress_callback = [](float progress, void * user_data){
        auto own = static_cast<LlamaWorker*>(user_data);
        if(own){
            own->_onUpdateProgress(progress);
        }
        return true;
    };

    mModel = llama_model_load_from_file(gguf.toLocal8Bit().data(), mp);
    if(!mModel){
        mIsError = true;
        emit modelError();
        emit sendStatus("Error of loading model", false);
        releaseModel();
        return;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = params.context;
    ctx_params.n_batch = 512;
    ctx_params.n_ubatch = 512;
    ctx_params.flash_attn_type = params.use_flashatt? LLAMA_FLASH_ATTN_TYPE_ENABLED : LLAMA_FLASH_ATTN_TYPE_DISABLED;
    ctx_params.type_k = params.type_k;
    ctx_params.type_v = params.type_v;

    mCtx = llama_init_from_model(mModel, ctx_params);
    if(!mCtx){
        emit modelError();
        emit sendStatus("Error init context", false);
        mIsError = true;
        releaseModel();
        return;
    }

    mVocab = llama_model_get_vocab(mModel);

    mSmpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(mSmpl, llama_sampler_init_min_p(mParams.min_p, 1));
    llama_sampler_chain_add(mSmpl, llama_sampler_init_temp(mParams.temp));
    llama_sampler_chain_add(mSmpl, llama_sampler_init_dist(mParams.seed));

    mBatch = llama_batch_init(mParams.n_batch, 0, 1);

    mChatTemplates = common_chat_templates_init(mModel, {});
    if(!mChatTemplates){
        emit modelError();
        emit sendStatus("Error init chat templates", false);
        mIsError = true;
        releaseModel();
        return;
    }

    if(!common_chat_templates_support_enable_thinking(mChatTemplates.get())){
        emit sendStatus("Thinking is not supported", false);
    }else{
        emit sendStatus("Thinking is supported", true);
    }

    emit modelLoaded();

    mIsLoading = false;
}

void LlamaWorker::unloadModel()
{
    releaseModel();
}

void LlamaWorker::releaseModel()
{
    mIsLoading = false;
    bool isLoaded = mModel != nullptr;
    clearMessage();
    if(mChatTemplates){
        mChatTemplates.reset();
    }
    llama_batch_free(mBatch);

    if(mSmpl){
        llama_sampler_free(mSmpl);
    }
    if(mCtx){
        llama_free(mCtx);
    }
    if(mModel){
        llama_model_free(mModel);
    }
    mBatch = {};
    mSmpl = nullptr;
    mModel = nullptr;
    mCtx = nullptr;
    mVocab = nullptr;

    if(isLoaded){
        emit modelUnloaded();
    }
}

void LlamaWorker::clearMessage()
{
    // for(auto &msg: mMessages){
    //     free(const_cast<char*>(msg.content));
    // }
    mPrevLen = {};
    mMessages = {};
}

void LlamaWorker::addNewMessage(const QString &user)
{
    if(!mModel || !mCtx){
        emit sendStatus("Model is not loaded or context is not init", false);
        return;
    }
    common_chat_templates_inputs inputs;

    mIsGen.exchange(true);
    mStop.exchange(false);

    common_chat_msg msg;
    msg.role = "user";
    msg.content = user.toStdString();
    mMessages.push_back(std::move(msg));

    inputs.messages = mMessages;
    inputs.add_generation_prompt = true;
    inputs.use_jinja = true;
    inputs.enable_thinking = mUseThinking;

    common_chat_params params;

    try{
        params = common_chat_templates_apply(mChatTemplates.get(), inputs);
    }catch(const std::exception &e){
        emit sendStatus(QString("Failed to apply chat templates %1")
                            .arg(QString::fromLatin1(e.what())), false);
        return;
    }

    auto formatted = params.prompt;

    auto previous_size = mPreviousFormatted.size();
    if(previous_size > formatted.size() ||
        formatted.compare(0, previous_size, mPreviousFormatted) != 0)
    {
        llama_memory_clear(llama_get_memory(mCtx), true);
        previous_size = 0;
    }

    std::string prompt = formatted.substr(previous_size);

    auto response = generate(prompt);

    if(!response.empty()){
        common_chat_msg assistant{};
        assistant.role = "assistant";
        assistant.content = response;
        mMessages.push_back(std::move(assistant));
    }

    common_chat_templates_inputs history_inputs;
    history_inputs.messages = mMessages;
    history_inputs.use_jinja = true;
    history_inputs.enable_thinking = mUseThinking;

    try{
        mPreviousFormatted = common_chat_templates_apply(mChatTemplates.get(), history_inputs).prompt;
    }catch(const std::exception &e){
        emit sendStatus(QString("Failed to format conversation history %1")
                            .arg(QString::fromLatin1(e.what())), false);
        return;

    }

    mIsGen.exchange(false);
}

void LlamaWorker::stopGenerate()
{
    mStop.exchange(true);
}

void LlamaWorker::_onUpdateProgress(float val)
{
    emit sendProgress(val);
}

void LlamaWorker::run()
{
    exec();
}

std::string LlamaWorker::generate(const std::string prompt)
{
    std::string response;
    Utf8Hold hold;

    QElapsedTimer token_timer, timeout;
    token_timer.start();

    auto is_first = llama_memory_seq_pos_max(llama_get_memory(mCtx), 0) == -1;

    llama_token n_cur;

    if(!stage_prefill(prompt, n_cur)){
        return {};
    }

    std::string piece;

    emit sendPiece("[Begin]\n");

    int tokens = 0;
    timeout.start();
    token_timer.start();
    while(!mStop.load()){
        llama_token new_token = llama_sampler_sample(mSmpl, mCtx, -1);
        if(llama_vocab_is_eog(mVocab, new_token)){
            break;
        }
        llama_sampler_accept(mSmpl, new_token);

        char buf[256];
        std::string piece;
        const int n = llama_token_to_piece(mVocab, new_token, buf, sizeof(buf), 0, true);
        if(n < 0){
            qWarning("need more n");
        }
        piece = hold.append(buf, n);
        if(!piece.empty()){
            emit sendPiece(QString::fromStdString(piece));
            response += piece;
        }
        if(timeout.elapsed() > 300){
            timeout.restart();
            emit sendMessage(QString::fromStdString(response));

            mGenSpeed = token_timer.elapsed() > 0? 1000. * tokens / token_timer.elapsed() : 0;
            emit sendStatistics(mPrefillSpeed, mGenSpeed);
        }

        mBatch.n_tokens = 1;
        mBatch.token[0] = new_token;
        mBatch.pos[0] = n_cur++;
        mBatch.n_seq_id[0] = 1;
        mBatch.seq_id[0][0] = 0;
        mBatch.logits[0] = 1;

        int ret = llama_decode(mCtx, mBatch);
        if(ret != 0){
            emit sendStatus("Error generate new token", false);
            break;
        }
        tokens++;
    }

    emit sendStatistics(mPrefillSpeed, mGenSpeed);

    piece = hold.flush();
    if(!piece.empty()){
        emit sendPiece(QString::fromStdString(piece));
        response += piece;
        emit sendMessage(QString::fromStdString(response));
    }

    if(mStop.load()){
        emit stopGenerated();
    }

    emit sendMessage(QString::fromStdString(response));
    emit generateFinished();
    emit stopGenerated();

    return response;
}

bool LlamaWorker::stage_prefill(const std::string& prompt, llama_token& n_prompt_total)
{
    auto n_prompt_tokens = -llama_tokenize(mVocab, prompt.c_str(), prompt.size(),
                                           nullptr, 0, true, true);
    if(n_prompt_tokens <= 0){
        emit sendStatus("Error tokenize", false);
        return false;
    }

    QElapsedTimer timer;
    timer.start();

    n_prompt_total = {};

    std::vector<llama_token> prompt_tokens(n_prompt_tokens);
    llama_pos n_past{};
    int prompt_offset = 0;
    int n_batch = mParams.n_batch;

    llama_tokenize(mVocab, prompt.data(), prompt.size(), prompt_tokens.data(), prompt_tokens.size(), true, true);

    while(prompt_offset < n_prompt_tokens){
        const int n_tokens = std::min(n_batch, n_prompt_tokens - prompt_offset);
        mBatch.n_tokens = 0;
        for(int i = 0; i < n_tokens; ++i){
            mBatch.token[i] = prompt_tokens[prompt_offset + i];
            mBatch.pos[i] = n_past + i;
            mBatch.n_seq_id[i] = 1;
            mBatch.seq_id[i][0] = 0;
            mBatch.logits[i] = 0;
        }
        mBatch.n_tokens = n_tokens;
        if(prompt_offset + n_tokens == n_prompt_tokens){
            mBatch.logits[n_tokens - 1] = 1;
        }

        int ret = llama_decode(mCtx, mBatch);
        if(ret != 0){
            emit sendStatus("Error prefill", false);
            return false;
        }
        n_past += n_tokens;
        prompt_offset += n_tokens;

        mPrefillSpeed = timer.elapsed() > 0? 1000. * prompt_offset / timer.elapsed() : 0;
        emit sendStatistics(mPrefillSpeed, 0);
    }

    n_prompt_total = n_past;

    mPrefillSpeed = 1000. * prompt_tokens.size() / timer.elapsed();
    emit sendStatistics(mPrefillSpeed, 0);

    return true;
}
