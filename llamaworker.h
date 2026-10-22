#ifndef LLAMAWORKER_H
#define LLAMAWORKER_H

#include "chat.h"
#include <QObject>
#include <QThread>
#include <llama.h>
#include <atomic>
#include <functional>

class LlamaWorker: public QThread
{
    Q_OBJECT
public:
    struct Params{
        int context{8192};
        int maxtoken{7000};
        int gpu_layers{-1};
        int n_batch{512};

        bool enable_thinking{false};

        float min_p{0.05f};
        float temp{0.8f};
        int seed = LLAMA_DEFAULT_SEED;

        bool use_flashatt{true};
        ggml_type type_k = ggml_type::GGML_TYPE_F16;
        ggml_type type_v = ggml_type::GGML_TYPE_F16;

        llama_split_mode split_mode{llama_split_mode::LLAMA_SPLIT_MODE_LAYER};
        QStringList selectedDevices;
        QVector<float> splitBetweenDevices;

        QString system_prompt;
    };

    LlamaWorker();
    ~LlamaWorker();

    bool isModelLoaded() const { return mModel != nullptr; }
    bool isError() const { return mIsError; }
    bool isGen() const { return mIsGen.load(); }
    bool isLoading() const { return mIsLoading; }

    QStringList getDevices() const;

signals:
    void loadingProgress(float);
    void sendStatus(QString, bool ok);
    void sendMessage(QString);
    void sendPiece(QString);
    void sendProgress(float);
    void generateFinished();
    void modelLoaded();
    void modelUnloaded();
    void modelError();
    void stopGenerated();
    void sendStatistics(float prefill, float gen);

public slots:
    void loadModel(const QString& gguf, const LlamaWorker::Params& params);
    void unloadModel();
    void releaseModel();
    void clearMessage();
    void addNewMessage(const QString& system_prompt, const QString& user);
    void stopGenerate();
    void _onUpdateProgress(float val);

protected:
    void run();

private:
    Params mParams;
    bool mIsLoading{};
    llama_model *mModel{};
    llama_context *mCtx{};
    bool mUseThinking{true};
    common_chat_templates_ptr mChatTemplates;
    std::vector<common_chat_msg> mMessages;
    std::string mPreviousFormatted;
    int mPrevLen{};
    bool mIsError{};
    const llama_vocab *mVocab{};
    llama_sampler *mSmpl{};
    llama_batch mBatch{};
    std::atomic_bool mStop;
    std::atomic_bool mIsGen;
    float mPrefillSpeed{};
    float mGenSpeed{};

    std::string generate(const std::string prompt);

    bool stage_prefill(const std::string &prompt, llama_token &n_prompt_total);
};

#endif // LLAMAWORKER_H
