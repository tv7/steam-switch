#include "QtFetcher.h"

#include "../core/http.h"

#include <QByteArray>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include <optional>
#include <string>

namespace ss::ui {

void installQtFetcher() {
    http::setFetcher([](const std::string& url) -> std::optional<std::string> {
        // Each worker thread gets its own manager + event loop (QNAM is not
        // thread-safe across threads). core/ calls this off the GUI thread.
        QNetworkAccessManager nam;
        QNetworkRequest req{QUrl(QString::fromStdString(url))};
        req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = nam.get(req);

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(10000);  // 10s, like covers._TIMEOUT
        loop.exec();

        std::optional<std::string> out;
        if (reply->error() == QNetworkReply::NoError) {
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 200) {
                QByteArray data = reply->readAll();
                if (!data.isEmpty()) out = std::string(data.constData(), (size_t)data.size());
            }
        }
        reply->deleteLater();
        return out;
    });
}

}  // namespace ss::ui
