/*
   Copyright 2013-2014 EditShare
   Copyright 2013-2015 Skytechnology sp. z o.o.
   Copyright 2023      Leil Storage OÜ

   This file is part of SaunaFS.

   SaunaFS is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 3.

   SaunaFS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with SaunaFS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/platform.h"
#include "common/metadata.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "common/cwrap.h"
#include "common/datapack.h"
#include "slogger/slogger.h"
#include "protocol/SFSCommunication.h"

#define METADATA_FILENAME_TEMPL "metadata.sfs"
const char kMetadataFilename[] = METADATA_FILENAME_TEMPL;
const char kMetadataTmpFilename[] = METADATA_FILENAME_TEMPL ".tmp";
const char kMetadataEmergencyFilename[] = METADATA_FILENAME_TEMPL ".emergency";
#undef METADATA_FILENAME_TEMPL
#define METADATA_LEGACY_FILENAME_TEMPL "metadata.mfs"
const char kMetadataLegacyFilename[] = METADATA_LEGACY_FILENAME_TEMPL;
#undef METADATA_LEGACY_FILENAME_TEMPL
#define METADATA_ML_FILENAME_TEMPL "metadata_ml.sfs"
const char kMetadataMlFilename[] = METADATA_ML_FILENAME_TEMPL;
const char kMetadataMlTmpFilename[] = METADATA_ML_FILENAME_TEMPL ".tmp";
#undef METADATA_ML_FILENAME_TEMPL
#define CHANGELOG_FILENAME "changelog.sfs"
const char kChangelogFilename[] = CHANGELOG_FILENAME;
const char kChangelogTmpFilename[] = CHANGELOG_FILENAME ".tmp";
#undef CHANGELOG_FILENAME
#define CHANGELOG_ML_FILENAME "changelog_ml.sfs"
const char kChangelogMlFilename[] = CHANGELOG_ML_FILENAME;
const char kChangelogMlTmpFilename[] = CHANGELOG_ML_FILENAME ".tmp";
#undef CHANGELOG_ML_FILENAME
#define SESSIONS_ML_FILENAME "sessions_ml.sfs"
const char kSessionsMlFilename[] = SESSIONS_ML_FILENAME;
const char kSessionsMlTmpFilename[] = SESSIONS_ML_FILENAME ".tmp";
#undef SESSIONS_ML_FILENAME
#define SESSIONS_FILENAME "sessions.sfs"
const char kSessionsFilename[] = SESSIONS_FILENAME;
const char kSessionsTmpFilename[] = SESSIONS_FILENAME ".tmp";
#undef SESSIONS_FILENAME

std::unique_ptr<Lockfile> gMetadataLockfile;

uint64_t metadataGetVersion(const std::string& file) {
	int fd;
	char chkbuff[20];
	char eofmark[16];

	fd = open(file.c_str(), O_RDONLY);
	if (fd<0) {
		throw MetadataCheckException("Can't open the metadata file");
	}

	int bytes = read(fd,chkbuff,20);

	if (bytes < 8) {
		close(fd);
		throw MetadataCheckException("Can't read the metadata file");
	}
	if (memcmp(chkbuff,"SFSM NEW",8)==0) {
		close(fd);
		return 0;
	}
	if (bytes != 20) {
		close(fd);
		throw MetadataCheckException("Can't read the metadata file");
	}

	std::string signature = std::string(chkbuff, 8);
	std::string sfsSignature = std::string(SFSSIGNATURE "M 2.9");
	std::string sauSignature = std::string(SAUSIGNATURE "M 2.9");
	std::string legacySignature = std::string("LIZM 2.9");

	if (signature == sfsSignature || signature == sauSignature) {
		memcpy(eofmark,"[SFS EOF MARKER]",16);
	} else if (signature == legacySignature) {
		safs_pretty_syslog(LOG_WARNING,
		                   "Legacy metadata section header %s, was detected in the metadata file %s",
		                   legacySignature.c_str(), file.c_str());
		memcpy(eofmark,"[MFS EOF MARKER]",16);
	} else {
		close(fd);
		throw MetadataCheckException("Bad EOF MARKER in the metadata file.");
	}

	const uint8_t* ptr = reinterpret_cast<const uint8_t*>(chkbuff + 8 + 4);
	uint64_t version;
	version = get64bit(&ptr);
	lseek(fd,-16,SEEK_END);
	if (read(fd,chkbuff,16)!=16) {
		close(fd);
		throw MetadataCheckException("Can't read the metadata file");
	}
	if (memcmp(chkbuff,eofmark,16)!=0) {
		close(fd);
		throw MetadataCheckException("The metadata file is truncated");
	}
	close(fd);
	return version;
}

uint64_t changelogGetFirstLogVersion(const std::string& fname) {
	uint8_t buff[50];
	int32_t s,p;
	uint64_t fv;
	int fd;

	fd = open(fname.c_str(), O_RDONLY);
	if (fd<0) {
		return 0;
	}
	s = read(fd,buff,50);
	close(fd);
	if (s<=0) {
		return 0;
	}
	fv = 0;
	p = 0;
	while (p<s && buff[p]>='0' && buff[p]<='9') {
		fv *= 10;
		fv += buff[p]-'0';
		p++;
	}
	if (p>=s || buff[p]!=':') {
		return 0;
	}
	return fv;
}

uint64_t changelogGetLastLogVersion(const std::string& fname) {
	struct stat st;

	FileDescriptor fd(open(fname.c_str(), O_RDONLY));
	if (fd.get() < 0) {
		throw FilesystemException("open " + fname + " failed: " + errorString(errno));
	}
	fstat(fd.get(), &st);

	size_t fileSize = st.st_size;
	if (fileSize == 0) {
		return 0;
	}

	const char* fileContent = (const char*) mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd.get(), 0);
	if (fileContent == MAP_FAILED) {
		throw FilesystemException("mmap(" + fname + ") failed: " + errorString(errno));
	}
	uint64_t lastLogVersion = 0;
	// first LF is (should be) the last byte of the file
	if (fileSize == 0 || fileContent[fileSize - 1] != '\n') {
		throw ParseException("truncated changelog " + fname +
				" (no LF at the end of the last line)");
	} else {
		size_t pos = fileSize - 1;
		while (pos > 0) {
			--pos;
			if (fileContent[pos] == '\n') {
				break;
			}
		}
		char *endPtr = NULL;
		lastLogVersion = strtoull(fileContent + pos, &endPtr, 10);
		if (*endPtr != ':') {
			throw ParseException("malformed changelog " + fname +
					" (expected colon after change number)");
		}
	}
	if (munmap((void*) fileContent, fileSize)) {
		safs_pretty_errlog(LOG_WARNING, "munmap(%s) failed", fname.c_str());
	}
	return lastLogVersion;
}

void changelogsMigrateFrom_1_6_29(const std::string& fname) {
	std::string name_new, name_old;
	for (uint32_t i = 0; i < 99; i++) {
		// 99 is the maximum number of changelog file in versions up to 1.6.29.
		name_old = fname + "." + std::to_string(i) + ".sfs";
		name_new = fname + ".sfs";
		if (i != 0) {
			name_new += "." + std::to_string(i);
		}
		try {
			if (fs::exists(name_old)) {
				if (!fs::exists(name_new)) {
					fs::rename(name_old, name_new);
				} else {
					safs_pretty_syslog(LOG_WARNING,
							"migrating changelogs from version 1.6.29: "
							"both old and new changelog files exist (%s and %s); "
							"old changelog won't be renamed automatically, "
							"fix this manually to remove this warning",
							name_old.c_str(), name_new.c_str());
				}
			}
		} catch (const FilesystemException& ex) {
			throw FilesystemException(
					"error when migrating changelogs from version 1.6.29: " + ex.message());
		}
	}
}
