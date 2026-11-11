#ifndef LLAMAWORKER_H
#define LLAMAWORKER_H

#include "chat.h"
#include <QObject>
#include <QThread>
#include <QVariant>
#include <QDataStream>
#include <llama.h>
#include <atomic>
#include <QTextDocument>
#include <QJsonObject>
#include <qtextcursor.h>
#include <queue>

class Content{
public:
    QString caption;
    //QString content;
    QByteArray json;
    std::vector<common_chat_msg> context;

    QSharedPointer<QTextDocument> _formatted;

    bool empty() const { return json.isEmpty(); }

    QTextDocument *doc(){
        if(!_formatted) _formatted.reset(new QTextDocument);
        return _formatted.get();
    }
    void setHtml(const QString& html){
        doc()->setHtml(html);
    }
    QString toHtml() const{
        if(_formatted) return _formatted->toHtml();
        return {};
    }
    void insertPlainText(const QString &text){
        QTextCursor cursor(doc());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(text);
    }
    void insertHtml(const QString &html)
    {
        QTextCursor cursor(doc());
        cursor.movePosition(QTextCursor::End);
        cursor.insertHtml(html);
    }

    QVariant toVariant() const{
        return QVariant::fromValue(*this);
    }
    static Content fromVariant(const QVariant& v){
        return v.value<Content>();
    }    
};

inline QDataStream& operator<<(QDataStream& out, const Content& c)
{
    out << c.caption;
    out << c.toHtml();
    out << (int)c.context.size();
    for(const auto &v: c.context){
        out << QString::fromStdString(v.role) << QString::fromStdString(v.content);
    }
    out << c.json;
    return out;
}

inline QDataStream& operator>>(QDataStream& in, Content& c)
{
    in >> c.caption;
    QString content;
    in >> content;
    c.setHtml(content);
    int ic{};
    in >> ic;
    if(ic < 0 || ic > 1e9){
        qDebug() << "Content reading: something wrong";
        return in;
    }
    for(int i = 0; i < ic; ++i){
        QString c1, c2;
        in >> c1 >> c2;
        common_chat_msg msg;
        msg.role = c1.toStdString();
        msg.content = c2.toStdString();
        c.context.push_back(msg);
    }
    in >> c.json;
    return in;
}

Q_DECLARE_METATYPE(Content)

const QMap<QString, ggml_type> KVCacheType = {
    {"fp16", ggml_type::GGML_TYPE_F16},
    {"q8_0", ggml_type::GGML_TYPE_Q8_0},
    {"q5_1", ggml_type::GGML_TYPE_Q5_1},
    {"q5_0", ggml_type::GGML_TYPE_Q5_0},
    {"q4_1", ggml_type::GGML_TYPE_Q4_1},
    {"q4_0", ggml_type::GGML_TYPE_Q4_0},
};

class LlamaWorker: public QThread
{
    Q_OBJECT
public:
    struct Params{
        int context{8192};
        int maxtoken{7000};
        int gpu_layers{-1};
        int n_batch{2048};
        int main_gpu{0};
        int n_threads{2};

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

        Params(){
            n_threads = std::max<int>(1, std::thread::hardware_concurrency() / 2);
        }
    };

    LlamaWorker();
    ~LlamaWorker();

    bool isModelLoaded() const { return mModel != nullptr; }
    bool isError() const { return mIsError; }
    bool isGen() const { return mIsGen.load(); }
    bool isLoading() const { return mIsLoading; }

    float contextProgress() const;

    QStringList getDevices() const;
    std::map<QString, ggml_backend_dev_t> getDevicesIdByName() const;

    void swapMessages(std::vector<common_chat_msg> &messages);
    std::vector<common_chat_msg> messages() const { return mMessages; }

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
    void sendReasoning(QString);
    void sendContent(QString);

public slots:
    void loadModel(const QString& gguf, const LlamaWorker::Params& params);
    void unloadModel();
    void releaseModel();
    void clearMessage();
    void addNewMessage(QString system_prompt, QString user);
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
    common_chat_parser_params mSyntax;
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
    float mContextProgress{};
    std::queue< std::tuple<QString, QString> > mUserQueue;

    std::string generate(const std::string prompt);

    bool stage_prefill(const std::string &prompt, llama_token &n_prompt_total);
};

#endif // LLAMAWORKER_H
