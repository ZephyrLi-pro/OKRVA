#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QSettings>

class ConfigManager : public QObject
{
    Q_OBJECT
    // 新手引导是否完成（is-first-install为false表示已完成）
    Q_PROPERTY(bool isGuideCompleted READ guideCompleted NOTIFY isGuideCompletedChanged)
    // 5.0新手引导是否完成（is-first-install-50为false表示已完成）
    Q_PROPERTY(bool isGuide50Completed READ guide50Completed NOTIFY isGuide50CompletedChanged)
public:
    // 获取单例实例
    static ConfigManager* getInstance();
    
    // 析构函数
    ~ConfigManager();

    // 获取配置值
    QVariant getConfigValue(const QString &key, const QVariant &defaultValue = QVariant()) const;

    // 修改配置值
    void updateConfig(const QString &key, const QVariant &value);
    // 修改非关键配置值
    void updateNonCriticalConfig(const QString &key, const QVariant &value);
    // 获取新手引导是否完成
    bool guideCompleted() const;
    // 获取5.0新手引导是否完成
    bool guide50Completed() const;

signals:
    // 新手引导完成状态变化信号
    void isGuideCompletedChanged();
    // 5.0新手引导完成状态变化信号
    void isGuide50CompletedChanged();

private:
    // 私有构造函数，禁止外部创建实例
    explicit ConfigManager(const QString &configFilePath, QObject *parent = nullptr);
    
    // 禁用拷贝构造函数和赋值运算符
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // 静态单例实例
    static ConfigManager* m_instance;
    
    QSettings *m_settings;  // QSettings 对象，用于操作 INI 配置文件
    bool m_isGuideCompleted = false;  // 缓存的新手引导完成状态
    bool m_isGuide50Completed = false;  // 缓存的5.0新手引导完成状态

    // 创建默认配置文件
    void createDefaultConfig();
    // 更新新手引导完成状态
    void updateGuideCompletedState();
    // 更新5.0新手引导完成状态
    void updateGuide50CompletedState();
    // 新手引导未完成时，强制配置为窗口模式
    void ensureWindowModeForIncompleteGuide();
};

#endif // CONFIGMANAGER_H
