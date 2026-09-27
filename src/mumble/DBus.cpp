// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "DBus.h"

#include "ACL.h"
#include "Channel.h"
#include "ClientUser.h"
#include "MainWindow.h"
#include "ServerHandler.h"
#include "Global.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QReadLocker>
#include <QtCore/QUrlQuery>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

#include <algorithm>

MumbleDBus::MumbleDBus(QObject *mw) : QDBusAbstractAdaptor(mw) {
}

void MumbleDBus::openUrl(const QString &url, const QDBusMessage &msg) {
	QUrl u     = QUrl::fromEncoded(url.toLatin1());
	bool valid = u.isValid();
	valid      = valid && (u.scheme() == QLatin1String("mumble"));
	if (!valid) {
		QDBusConnection::sessionBus().send(
			msg.createErrorReply(dbusErrorPrefix() + QLatin1String(".url"), QLatin1String("Invalid URL")));
	} else {
		Global::get().mw->openUrl(u);
	}
}

void MumbleDBus::getCurrentUrl(const QDBusMessage &msg) {
	if (!Global::get().sh || !Global::get().sh->isRunning() || !Global::get().uiSession) {
		QDBusConnection::sessionBus().send(
			msg.createErrorReply(dbusErrorPrefix() + QLatin1String(".connection"), QLatin1String("Not connected")));
		return;
	}
	QString host, user, pw;
	unsigned short port;
	QUrl u;

	Global::get().sh->getConnectionInfo(host, port, user, pw);
	u.setScheme(QLatin1String("mumble"));
	u.setHost(host);
	u.setPort(port);
	u.setUserName(user);

	QUrlQuery query;
	query.addQueryItem(QLatin1String("version"), QLatin1String("1.2.0"));
	u.setQuery(query);

	QStringList path;
	Channel *c = ClientUser::get(Global::get().uiSession)->cChannel;
	while (c->cParent) {
		path.prepend(c->qsName);
		c = c->cParent;
	}
	QString fullpath = path.join(QLatin1String("/"));
	// Make sure fullpath starts with a slash for non-empty paths. Setting
	// a path without a leading slash clears the whole QUrl.
	if (!fullpath.isEmpty()) {
		fullpath.prepend(QLatin1String("/"));
	}
	u.setPath(fullpath);
	QDBusConnection::sessionBus().send(msg.createReply(QString::fromLatin1(u.toEncoded())));
}

void MumbleDBus::getTalkingUsers(const QDBusMessage &msg) {
	if (!Global::get().sh || !Global::get().sh->isRunning() || !Global::get().uiSession) {
		QDBusConnection::sessionBus().send(
			msg.createErrorReply(dbusErrorPrefix() + QLatin1String(".connection"), QLatin1String("Not connected")));
		return;
	}
	QStringList names;
	for (ClientUser *cu : ClientUser::getTalking()) {
		names.append(cu->qsName);
	}
	QDBusConnection::sessionBus().send(msg.createReply(names));
}

void MumbleDBus::focus() {
	Global::get().mw->show();
	Global::get().mw->raise();
	Global::get().mw->activateWindow();
}

void MumbleDBus::setTransmitMode(unsigned int mode, const QDBusMessage &msg) {
	switch (mode) {
		case 0:
			Global::get().s.atTransmit = Settings::Continuous;
			break;
		case 1:
			Global::get().s.atTransmit = Settings::VAD;
			break;
		case 2:
			Global::get().s.atTransmit = Settings::PushToTalk;
			break;
		default:
			QDBusConnection::sessionBus().send(msg.createErrorReply(dbusErrorPrefix() + QLatin1String(".transmitMode"),
																	QLatin1String("Invalid transmit mode")));
			return;
	}
	QMetaObject::invokeMethod(Global::get().mw, "updateTransmitModeComboBox", Qt::QueuedConnection);
}

unsigned int MumbleDBus::getTransmitMode() {
	return Global::get().s.atTransmit;
}

void MumbleDBus::toggleSelfMuted() {
	Global::get().mw->qaAudioMute->trigger();
}

void MumbleDBus::toggleSelfDeaf() {
	Global::get().mw->qaAudioDeaf->trigger();
}

void MumbleDBus::setSelfMuted(bool mute) {
	Global::get().mw->qaAudioMute->setChecked(!mute);
	Global::get().mw->qaAudioMute->trigger();
}

void MumbleDBus::setSelfDeaf(bool deafen) {
	Global::get().mw->qaAudioDeaf->setChecked(!deafen);
	Global::get().mw->qaAudioDeaf->trigger();
}

bool MumbleDBus::isSelfMuted() {
	return Global::get().s.bMute;
}

bool MumbleDBus::isSelfDeaf() {
	return Global::get().s.bDeaf;
}

void MumbleDBus::startTalking() {
	Global::get().mw->on_PushToTalk_triggered(true, QVariant());
}

void MumbleDBus::stopTalking() {
	Global::get().mw->on_PushToTalk_triggered(false, QVariant());
}

void MumbleDBus::startWhisper(const QString &channel, const QDBusMessage &msg) {
	startWhisper(channel, false, false, false, QString(), msg);
}

void MumbleDBus::startWhisper(const QString &channel, bool subchannels, bool links, bool forceCenter,
							  const QString &group, const QDBusMessage &msg) {
	if (!Global::get().mw->setRpcWhispering(true, channel, 0, false, subchannels, links, forceCenter, group)) {
		QDBusConnection::sessionBus().send(
			msg.createErrorReply(dbusErrorPrefix() + QLatin1String(".whisper"), QLatin1String("Unable to start whisper")));
	}
}

void MumbleDBus::stopWhisper(const QDBusMessage &msg) {
	stopWhisper(QString(), false, false, false, QString(), msg);
}

void MumbleDBus::stopWhisper(const QString &channel, bool subchannels, bool links, bool forceCenter,
							 const QString &group, const QDBusMessage &msg) {
	if (!Global::get().mw->setRpcWhispering(false, channel, 0, false, subchannels, links, forceCenter, group)) {
		QDBusConnection::sessionBus().send(
			msg.createErrorReply(dbusErrorPrefix() + QLatin1String(".whisper"), QLatin1String("Unable to stop whisper")));
	}
}

void MumbleDBus::startShout(const QString &channel, const QDBusMessage &msg) {
	startWhisper(channel, true, false, false, QString(), msg);
}

void MumbleDBus::startShout(const QString &channel, bool links, bool forceCenter, const QString &group,
							const QDBusMessage &msg) {
	startWhisper(channel, true, links, forceCenter, group, msg);
}

void MumbleDBus::stopShout(const QDBusMessage &msg) {
	stopWhisper(msg);
}

void MumbleDBus::getChannelTree(const QDBusMessage &msg) {
	if (!Global::get().sh || !Global::get().sh->isRunning() || !Global::get().uiSession) {
		QDBusConnection::sessionBus().send(
			msg.createErrorReply(dbusErrorPrefix() + QLatin1String(".connection"), QLatin1String("Not connected")));
		return;
	}

	auto channelPath = [](const Channel *c) -> QString {
		QStringList path;
		while (c && c->cParent) {
			path.prepend(c->qsName);
			c = c->cParent;
		}
		return QLatin1Char('/') + path.join(QLatin1Char('/'));
	};

	QReadLocker lock(&Channel::c_qrwlChannels);

	QList< Channel * > channels = Channel::c_qhChannels.values();
	std::sort(channels.begin(), channels.end(),
			  [&](const Channel *a, const Channel *b) { return channelPath(a) < channelPath(b); });

	QJsonArray jsonChannels;
	for (const Channel *c : channels) {
		QJsonObject obj;
		obj.insert(QLatin1String("id"), static_cast< qint64 >(c->iId));
		obj.insert(QLatin1String("path"), channelPath(c));
		if (c->cParent) {
			obj.insert(QLatin1String("parent"), static_cast< qint64 >(c->cParent->iId));
		}
		obj.insert(QLatin1String("users"), c->qlUsers.count());
		if (c->bTemporary) {
			obj.insert(QLatin1String("temporary"), true);
		}

		if (!c->qsPermLinks.isEmpty()) {
			QJsonArray links;
			for (const Channel *l : c->qsPermLinks) {
				links.append(channelPath(l));
			}
			obj.insert(QLatin1String("links"), links);
		}

		// Permissions are only known for channels the server has sent them for
		if (c->uiPermissions & ChanACL::Cached) {
			obj.insert(QLatin1String("whisper"), (c->uiPermissions & ChanACL::Whisper) != 0);
		}

		jsonChannels.append(obj);
	}

	QJsonObject result;
	result.insert(QLatin1String("current"),
				  channelPath(ClientUser::get(Global::get().uiSession)->cChannel));
	result.insert(QLatin1String("channels"), jsonChannels);

	QDBusConnection::sessionBus().send(
		msg.createReply(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))));
}

void MumbleDBus::requestChannelPermissions(unsigned int channelID, const QDBusMessage &msg) {
	if (!Global::get().sh || !Global::get().sh->isRunning() || !Global::get().uiSession) {
		QDBusConnection::sessionBus().send(
			msg.createErrorReply(dbusErrorPrefix() + QLatin1String(".connection"), QLatin1String("Not connected")));
		return;
	}

	if (!Channel::get(channelID)) {
		QDBusConnection::sessionBus().send(
			msg.createErrorReply(dbusErrorPrefix() + QLatin1String(".channel"), QLatin1String("Unknown channel")));
		return;
	}

	Global::get().sh->requestChannelPermissions(channelID);
}
