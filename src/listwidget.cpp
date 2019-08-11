/*-
 * Copyright (c) 2019 Marcel Kaiser. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <QMimeData>
#include <QDebug>

#include "listwidget.h"

ListWidget::ListWidget(QWidget* parent) : QListWidget(parent)
{
	this->setAcceptDrops(true);
	this->setDropIndicatorShown(true);
}

void ListWidget::dropEvent(QDropEvent* event)
{
	if (!event->mimeData()->hasFormat("text/uri-list")) {
		event->ignore();
		return;
	}
	event->accept();
	QByteArray data = event->mimeData()->data("text/uri-list");
	QList <QByteArray> lines = data.split('\n');
	QStringList files;
	for (int i = 0; i < lines.count(); i++) {
		QByteArray ba(lines.at(i));
		if (ba.endsWith('\r'))
			ba.chop(1);
		if (!ba.startsWith("file://"))
			continue;
		ba.remove(0, 7);
		if (!ba.isEmpty())
			files += ba;
	}
	emit itemDroped(files);
}

void ListWidget::dragEnterEvent(QDragEnterEvent* event)
{
        event->accept();
}

void ListWidget::dragMoveEvent(QDragMoveEvent* e)
{
	if (e->source() != this) {
		e->accept();
	} else
		e->ignore();
}

void ListWidget::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Delete) {
		emit deleteKeyPressed();
		event->accept();
	} else
		QListView::keyPressEvent(event);
}
