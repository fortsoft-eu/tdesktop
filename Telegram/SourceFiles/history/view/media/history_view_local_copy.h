/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_file_origin.h"
#include "ui/text/text_entity.h"

class DocumentData;
class PhotoData;
class HistoryItem;
class QWidget;

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class Show;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace HistoryView {

struct LocalMediaCopy {
	QByteArray bytes;
	QString path;
	QString name;
	DocumentData *document = nullptr;
	PhotoData *existingPhoto = nullptr;
	bool photo = false;
	Data::FileOrigin origin;
};

struct LocalMessageCopy {
	TextWithTags text;
	std::optional<LocalMediaCopy> media;
};

[[nodiscard]] bool HasLocalMediaCopy(not_null<DocumentData*> document);
[[nodiscard]] bool HasLocalMediaCopy(not_null<PhotoData*> photo);

[[nodiscard]] std::optional<LocalMediaCopy> ReadLocalMediaCopy(
	not_null<DocumentData*> document);
[[nodiscard]] std::optional<LocalMediaCopy> ReadLocalMediaCopy(
	not_null<PhotoData*> photo);
[[nodiscard]] std::optional<LocalMessageCopy> ReadLocalMessageCopy(
	not_null<HistoryItem*> item,
	bool allowDownload = false);
[[nodiscard]] bool WriteLocalMediaCopy(
	const LocalMediaCopy &copy,
	const QString &path);
void SaveLocalMediaCopy(
	LocalMediaCopy copy,
	not_null<Main::Session*> session,
	std::shared_ptr<Ui::Show> show,
	QWidget *parent = nullptr);
void ShowSendLocalCopy(
	not_null<Window::SessionController*> controller,
	LocalMessageCopy copy);
void ShowSendLocalCopies(
	not_null<Window::SessionController*> controller,
	MessageIdsList ids);

} // namespace HistoryView
