// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "MainWindow.h"
#include "Global.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QSocketNotifier>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

static int signalFds[2] = { -1, -1 };

static void handleTerminationSignal(int) {
	// Only async-signal-safe calls are allowed here, so just wake up the event loop.
	const char c                           = 1;
	[[maybe_unused]] const ssize_t written = ::write(signalFds[0], &c, sizeof(c));
}

/// Shut down cleanly on SIGTERM/SIGINT/SIGHUP (e.g. when a systemd unit is stopped on logout) so the
/// settings get saved and the next start does not complain that Mumble did not quit normally.
static void installTerminationHandler() {
	if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, signalFds) != 0) {
		qWarning("Failed to create socket pair for signal handling");
		return;
	}

	QSocketNotifier *notifier = new QSocketNotifier(signalFds[1], QSocketNotifier::Read, qApp);
	QObject::connect(notifier, &QSocketNotifier::activated, [notifier]() {
		char c;
		[[maybe_unused]] const ssize_t readBytes = ::read(signalFds[1], &c, sizeof(c));
		notifier->setEnabled(false);

		qWarning("Received termination signal, shutting down");
		if (Global::get().mw) {
			// Same as File -> Quit: skips the "are you sure" prompt and the minimize-on-close behavior
			Global::get().mw->forceQuit = true;
			Global::get().mw->close();
		} else {
			// The event loop is not running yet; main() checks this before entering it
			Global::get().bQuit = true;
		}
	});

	struct sigaction sa = {};
	sa.sa_handler       = handleTerminationSignal;
	sigemptyset(&sa.sa_mask);
	// SA_RESETHAND: a second signal kills Mumble immediately in case the clean shutdown hangs
	sa.sa_flags = static_cast< int >(SA_RESTART | SA_RESETHAND);
	for (int sig : { SIGTERM, SIGINT, SIGHUP }) {
		if (sigaction(sig, &sa, nullptr) != 0) {
			qWarning("Failed to install handler for signal %d", sig);
		}
	}
}

void os_init() {
	installTerminationHandler();
}
