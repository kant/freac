 /* fre:ac - free audio converter
  * Copyright (C) 2001-2018 Robert Kausch <robert.kausch@freac.org>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the GNU General Public License as
  * published by the Free Software Foundation, either version 2 of
  * the License, or (at your option) any later version.
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#include <jobs/joblist/adddirectory.h>

#include <cddb/cddbremote.h>

#include <config.h>
#include <utilities.h>

#include <boca.h>

using namespace BoCA;
using namespace BoCA::AS;

freac::JobAddDirectory::JobAddDirectory(const String &iDirectory) : JobAddFiles(Array<String>())
{
	directory = iDirectory;
}

freac::JobAddDirectory::~JobAddDirectory()
{
}

Void freac::JobAddDirectory::AddDirectory(const Directory &directory)
{
	if (!directory.Exists()) return;

	/* Recurse into subdirectories.
	 */
	const Array<Directory>	&myDirectories = directory.GetDirectories();
	const Array<File>	&myFiles       = directory.GetFiles();

	foreach (const Directory &directory, myDirectories) AddDirectory(directory);

	/* Find extensions to exclude and add files.
	 */
	String			 configString  = configuration->GetStringValue(Config::CategorySettingsID, Config::SettingsExcludeExtensionsID, Config::SettingsExcludeExtensionsDefault).ToLower();
	const Array<String>	&extensions    = configString.Explode("|");

	foreach (const File &file, myFiles)
	{
		String	 fileString = String(file).ToLower();
		Bool	 add	    = True;

		foreach (const String &extension, extensions)
		{
			if (fileString.EndsWith(extension))
			{
				add = False;

				break;
			}
		}

		if (add) files.Add(file);
	}

	String::ExplodeFinish();
}

Void freac::JobAddDirectory::RemoveReferencedFiles()
{
	/* Find and remove files referenced by
	 * cuesheets to avoid adding them twice.
	 */
	Registry	&boca = Registry::Get();

	for (Int i = 0; i < files.Length(); i++)
	{
		const String	&file = files.GetNth(i);

		if (!file.EndsWith(".cue")) continue;

		/* Get decoder for file.
		 */
		DecoderComponent	*decoder = boca.CreateDecoderForStream(file);

		if (decoder == NIL) continue;

		/* Get album info and loop over tracks.
		 */
		Track	 album;

		decoder->GetStreamInfo(file, album);

		foreach (const Track &track, album.tracks)
		{
			for (Int j = 0; j < files.Length(); j++)
			{
				const String	&file = files.GetNth(j);

				/* Remove file from list if filename matches.
				 */
				if (file == track.origFilename)
				{
					if (j < i) i--;

					files.RemoveNth(j--);
				}
			}
		}

		boca.DeleteComponent(decoder);
	}
}

Bool freac::JobAddDirectory::ReadyToRun()
{
	BoCA::JobList	*joblist = BoCA::JobList::Get();

	if (joblist->IsLocked()) return False;

	joblist->Lock();

	return True;
}

Error freac::JobAddDirectory::Perform()
{
	BoCA::I18n	*i18n = BoCA::I18n::Get();

	i18n->SetContext("Jobs::Joblist");
 
	SetText(i18n->AddEllipsis(i18n->TranslateString("Reading folders")));

	AddDirectory(directory);

	SetText(i18n->AddEllipsis(i18n->TranslateString("Filtering duplicates")));

	RemoveReferencedFiles();

	return JobAddFiles::Perform();
}
