// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "imguruploader.h"
#include "src/utils/confighandler.h"
#include "src/utils/filenamehandler.h"
#include "src/utils/history.h"
#include "src/widgets/loadspinner.h"
#include "src/widgets/notificationwidget.h"
#include <QBuffer>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QShortcut>
#include <QUrlQuery>
#include <QHttpPart>

ImgurUploader::ImgurUploader(const QPixmap& capture, QWidget* parent)
  : ImgUploaderBase(capture, parent)
{
    m_NetworkAM = new QNetworkAccessManager(this);
    connect(m_NetworkAM,
            &QNetworkAccessManager::finished,
            this,
            &ImgurUploader::handleReply);
}

void ImgurUploader::handleReply(QNetworkReply* reply)
{
    spinner()->deleteLater();
    m_currentImageName.clear();
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
        QJsonObject data = response.object();
        //QJsonObject data = json[QStringLiteral("data")].toObject();
        setImageURL(data[QStringLiteral("fileURL")].toString());


        auto deleteURL = data[QStringLiteral("deletionURL")].toString();
        auto deleteToken = deleteURL.section("/", -1, -1);

        // save history
        m_currentImageName = imageURL().toString();
        int lastSlash = m_currentImageName.lastIndexOf("/");
        if (lastSlash >= 0) {
            m_currentImageName = m_currentImageName.mid(lastSlash + 1);
        }

        // save image to history
        History history;
        m_currentImageName =
          history.packFileName("nestrip", deleteToken, m_currentImageName);
        history.save(pixmap(), m_currentImageName);

        emit uploadOk(imageURL());
    } else {
        setInfoLabelText(reply->errorString());
    }
    new QShortcut(Qt::Key_Escape, this, SLOT(close()));
}

void ImgurUploader::upload()
{
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    pixmap().save(&buffer, "PNG");

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart imagePart;
    imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/png"));
    imagePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"files\"; filename=\"image.png\""));
    imagePart.setBody(byteArray);

    multiPart->append(imagePart);

    QUrl url(QStringLiteral("https://nest.rip/api/files/upload"));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                         QStringLiteral("%1")
                           .arg(ConfigHandler().uploadClientSecret())
                           .toUtf8());

    m_NetworkAM->post(request, multiPart);
}

void ImgurUploader::deleteImage(const QString& fileName,
                                const QString& deleteToken)
{
    Q_UNUSED(fileName)
    bool successful = QDesktopServices::openUrl(
      QUrl(QStringLiteral("https://nest.rip/api/files/delete/%1").arg(deleteToken)));
    if (!successful) {
        notification()->showMessage(tr("Unable to open the URL."));
    }

    emit deleteOk();
}
