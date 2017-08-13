/*-
 * Copyright (c) 2016 Marcel Kaiser. All rights reserved.
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

#include <QMainWindow>
#include <QApplication>
#include <QStringList>
#include <QListWidget>
#include <QWidget>
#include <QObject>
#include "lib/dsbautostart.h"

class List : public QWidget {
Q_OBJECT

public:
	List(dsbautostart_t *as, QWidget *parent = 0);
	void addItem(entry_t *entry);
	void moveItemUp();
	void moveItemDown();
	void newItem();
	void delItem();
	void update();
	void undo();
	void redo();
	bool modified();
	bool canUndo();
	bool canRedo();
	void unsetModified();
signals:
	void listModified(bool);
private slots:
	void catchItemChanged(QListWidgetItem *item);
private:
	void compare();
private:
	bool _modified;
	dsbautostart_t *as, *ascp;
	QList<QListWidgetItem *> items;
	QListWidget *list;
	void updateItem(QListWidgetItem *item);
};

