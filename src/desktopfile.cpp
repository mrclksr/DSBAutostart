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

#include <QFile>
#include <QTextStream>

#include "desktopfile.h"

DesktopFile::DesktopFile(QString &path)
{
	this->path = path;
}

int DesktopFile::read()
{
	bool haveMagic = false;
	
	if (path.isEmpty())
		return (-1);
	QFile file(path);
	if (!file.exists())
		return (-1);
	if (!file.open(QIODevice::ReadOnly))
		return (-1);
	QTextStream in(&file);
	while (!in.atEnd()) {
		QString s = in.readLine();
		if (s.isEmpty())
			continue;
		if (!haveMagic) {
			if (s == "[Desktop Entry]")
				haveMagic = true;
			continue;
		}
		if (s.startsWith("Name="))
			name = s.mid(5);
		else if (s.startsWith("Exec="))
			cmd = s.mid(5);
	}
	file.close();
	if (!haveMagic || cmd.isEmpty())
		return (-1);
	return (0);
}
