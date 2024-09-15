/*  This is part of KokoVP

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include "playercontroller.h"
#include "extensions.h"
#include "playerwidget.h"
#include "helper.h"

PlayerController::PlayerController(PlayerWidget *parent)
    : QObject{parent}
{
    p = parent;
    prop("volume")->set(50);
    prop("pause")->set(true);
    p->setProp("audio-file-auto-exts", Extensions.audio());
    connect(p, &PlayerWidget::fileLoaded, this, &PlayerController::handleMediaLoad);
    connect(p, &PlayerWidget::endFile, this, &PlayerController::endMediaRessource);
    connect(p, &PlayerWidget::endFile, this, &PlayerController::handleMediaEnd);
}

PropertyObserver *PlayerController::prop(QString name)
{
    return p->propertyObserver(name);
}

void PlayerController::setProp(const QString &name, const QVariant &value)
{
    p->setProp(name, value);
}

QVariant PlayerController::getProp(const QString &name) const
{
    return p->getProp(name);
}

void PlayerController::setOption(const QString &name, const QVariant &value)
{
    p->setOption(name, value);
}

void PlayerController::handleMediaEnd()
{
    haveMediaUrl = false;
    if (!queuedMediaUrl.isEmpty())
    {
        QUrl url = queuedMediaUrl;
        queuedMediaUrl = QUrl();
        open(url);
    }
}

void PlayerController::open(const QUrl &url)
{
    if (haveMediaUrl)
    {
        queuedMediaUrl = url;
        return stop();
    }
    p_tracks.clear();

    // Here we need to scan siblings folder for possible external subtitles and audio
    //, then set it to sub-file-paths and audio-file-paths OPTIONs (not properties)
    if (url.isLocalFile())
    {
        QDir mediaDir = QFileInfo(url.toLocalFile()).absoluteDir();

        if (p_extSubMaxDepth>=0 && p_extSubMode!="no")
        {
            QStringList subsFolders;
            Helper::searchWithMaxDepth(subsFolders, Extensions.subtitles().forDirFilter(), mediaDir, p_extSubMaxDepth, false);
            p->setOption("sub-auto", p_extSubMode);
            p->setOption("sub-file-paths", subsFolders);
        }

        if (p_extAudioMaxDepth>=0 && p_extAudioMode!="no")
        {
            QStringList audioFolders;
            Helper::searchWithMaxDepth(audioFolders, Extensions.audio().forDirFilter(), mediaDir, p_extAudioMaxDepth, false);
            p->setOption("audio-file-auto", p_extAudioMode);
            p->setOption("audio-file-paths", audioFolders);
        }
    }

    p->command(QStringList{"loadfile", url.toString()});
}

void PlayerController::stop()
{
    const static QVariantList cmd = QVariantList({"stop"});
    p->command(cmd);
    p_tracks.clear();
    emit tracksUpdated();
}

void PlayerController::togglePlayback()
{
    p->setProp("pause", isPlaying());
}

void PlayerController::seekAbsolute(double s)
{
    p->command(QVariantList({"seek", s, "absolute"}));
}

void PlayerController::seekRelative(double s)
{
    p->command(QVariantList({"seek", s, "relative"}));
}

void PlayerController::setScreenshotOpts(const QString &dir, const QString &scrTemplate, const QString &format)
{
    p->setProp("screenshot-dir", dir);
    p->setProp("screenshot-template", scrTemplate);
    p->setProp("screenshot-format", format);
}

void PlayerController::screenshot(const QString &outPath, bool includeSubs)
{
    const char *scrFlags = includeSubs ? "subtitles" : "video";
    if (outPath.length()>0)
        p->command(QVariantList({"screenshot-to-file", outPath, scrFlags}));
    else
        p->command(QVariantList({"screenshot", scrFlags}));
}

void PlayerController::subSeek(int skip, bool secondary)
{
    p->command(QVariantList({"sub-seek", skip, secondary ? "secondary" : "primary"}));
}

void PlayerController::subStep(int skip, bool secondary)
{
    p->command(QVariantList({"sub-step", skip, secondary ? "secondary" : "primary"}));
}

void PlayerController::frameStep(int step) {
    const static QVariantList cmd_forward = QVariantList({"frame-step"});
    const static QVariantList cmd_back = QVariantList({"frame-back-step"});

    switch (step)
    {
    case 1:
        return p->command(cmd_forward);
    case -1:
        return p->command(cmd_back);
    };
}

bool PlayerController::isPlaying()
{
    return !getProp("pause").toBool();
}

void PlayerController::handleMediaLoad()
{
    lastMediaUrl = currentMediaUrl();
    haveMediaUrl = true;
    bool ok;
    int tracksCount = getProp("track-list/count").toInt(&ok);
    assert(ok);
    for (int i = 0; i<tracksCount; i++)
    {
        QString trackAddr = QString("track-list/%1/").arg(i);
        Track t;
        t.id = getProp(trackAddr + "id").toInt();
        t.title = getProp(trackAddr + "title").toString();
        t.lang = getProp(trackAddr + "lang").toString();
        t.isExternal = getProp(trackAddr + "external").toBool();
        QString type = getProp(trackAddr + "type").toString();
        if (type=="video")
            t.type = Track::TRACK_TYPE_VIDEO;
        else if (type=="audio")
            t.type = Track::TRACK_TYPE_AUDIO;
        else if (type=="sub")
            t.type = Track::TRACK_TYPE_SUB;

        if (t.isExternal)
            t.mediaUrl = p->getProp(trackAddr + "external-mediaUrl").toString();

        p_tracks.append(t);
    }
    //p->command(QVariantList({"vf", "clr", ""})); //SVP
    prop("aid")->set("auto");
    prop("vid")->set("auto");
    prop("sid")->set("auto");
    prop("secondary-sid")->set(-1);

    p->setProp("pause", false); // Previous file can be paused, when new file is loaded, it's time to de-pause player
    emit tracksUpdated();
    emit mediaMetaUpdated(p->getProp("media-title").toString(), prop("duration")->get().toDouble());
}
