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

#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QStatusBar>
#include <QPushButton>
#include <QGroupBox>
#include <QRadioButton>
#include <QWidget>
#include <QCheckBox>

#include "lib/dsbautostart.h"

class EditWin : public QDialog {
	Q_OBJECT
public:
	EditWin(entry_t *entry, QWidget *parent = 0);
private slots:
	void	     validate(void);
	void	     terminalToggled(int state);
	void 	     acceptSlot(void);
	void	     nsi_osi_rb_toggled(bool);
	QGroupBox    *createVisibilityBox(entry_t *entry);
public:
	bool	     terminal;
	QByteArray   name;
	QByteArray   command;
	QByteArray   comment;
	QByteArray   notShowIn;
	QByteArray   onlyShowIn;
private:
	QLineEdit    *name_edit;
	QLineEdit    *command_edit;
	QLineEdit    *comment_edit;
	QLineEdit    *list_le;
	QCheckBox    *terminal_cb;
	QStatusBar   *statusBar;
	QPushButton  *ok_pb;
	QRadioButton *nsi_rb;
	QRadioButton *osi_rb;
};
