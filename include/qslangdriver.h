#ifndef QSLANGDRIVER_H
#define QSLANGDRIVER_H

#include <QMap>
#include <QObject>

class QSlangDriver : public QObject
{
    Q_OBJECT
public:
    explicit QSlangDriver(QObject *parent = nullptr);
    ~QSlangDriver();

public slots:
    void setEnv(const QString &key, const QString &value);
    void setEnv(const QMap<QString, QString> &env);
    bool parseArgs(const QString &args);
    bool parseFileList(const QString &fileListName);

private:
    QMap<QString, QString> env;
};

#endif // QSLANGDRIVER_H