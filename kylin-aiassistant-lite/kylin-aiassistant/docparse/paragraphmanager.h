#ifndef PARAGRAPHMANAGER_H
#define PARAGRAPHMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QRandomGenerator>
#include <QQueue>

class ParagraphManager : public QObject
{
    Q_OBJECT

public:
    explicit ParagraphManager(QObject *parent = nullptr);
    ~ParagraphManager();

    // 加载段落数据
    bool loadParagraphs(const QString &filePath);

    // 获取下一个段落（确保每轮每个段落都显示一次）
    QString getNextParagraph();

    // 获取段落数量
    int paragraphCount() const;

    // 获取所有段落（用于调试）
    QStringList getAllParagraphs() const;

private:
    // 初始化新一轮的段落队列
    void initNewRound();

    QStringList m_paragraphs;
    QQueue<QString> m_currentRoundQueue;  // 当前轮的队列
    QStringList m_usedInCurrentRound;     // 当前轮已使用的段落
    bool m_loaded;
};

#endif // PARAGRAPHMANAGER_H
