// Archive/IsoIn.h

#ifndef __ARCHIVE_ISO_IN_H
#define __ARCHIVE_ISO_IN_H

#include "../../../Common/IntToString.h"
#include "../../../Common/MyCom.h"

#include "../../IStream.h"

#include "IsoHeader.h"
#include "IsoItem.h"

namespace NArchive {
namespace NIso {

struct CDir: public CDirRecord
{
  CDir *Parent;
  CObjectVector<CDir> _subItems;

  void Clear()
  {
    Parent = 0;
    _subItems.Clear();
  }
  
  unsigned GetLen(bool checkSusp, unsigned skipSize) const
  {
    unsigned len = GetLenCur(checkSusp, skipSize);
    if (Parent != 0)
      if (Parent->Parent != 0)
        len += 1 + Parent->GetLen(checkSusp, skipSize);
    return len;
  }

  unsigned GetLenU() const
  {
    unsigned len = (unsigned)(FileId.Size() / 2);
    if (Parent != 0)
      if (Parent->Parent != 0)
        len += 1 + Parent->GetLenU();
    return len;
  }
  
  AString GetPath(bool checkSusp, unsigned skipSize) const
  {
    AString s;
    unsigned len = GetLen(checkSusp, skipSize);
    char *p = s.GetBuffer(len);
    p += len;
    *p = 0;
    const CDir *cur = this;
    for (;;)
    {
      unsigned curLen = cur->GetLenCur(checkSusp, skipSize);
      p -= curLen;
      memcpy(p, (const char *)(const Byte *)cur->GetNameCur(checkSusp, skipSize), curLen);
      cur = cur->Parent;
      if (cur == 0)
        break;
      if (cur->Parent == 0)
        break;
      p--;
      *p = CHAR_PATH_SEPARATOR;
    }
    s.ReleaseBuffer();
    return s;
  }

  UString GetPathU() const
  {
    UString s;
    unsigned len = GetLenU();
    wchar_t *p = s.GetBuffer(len);
    p += len;
    *p = 0;
    const CDir *cur = this;
    for (;;)
    {
      unsigned curLen = (unsigned)(cur->FileId.Size() / 2);
      p -= curLen;
      for (unsigned i = 0; i < curLen; i++)
      {
        Byte b0 = ((const Byte *)cur->FileId)[i * 2];
        Byte b1 = ((const Byte *)cur->FileId)[i * 2 + 1];
        p[i] = (wchar_t)(((wchar_t)b0 << 8) | b1);
      }
      cur = cur->Parent;
      if (cur == 0)
        break;
      if (cur->Parent == 0)
        break;
      p--;
      *p = WCHAR_PATH_SEPARATOR;
    }
    s.ReleaseBuffer();
    return s;
  }
};

struct CDateTime
{
  UInt16 Year;
  Byte Month;
  Byte Day;
  Byte Hour;
  Byte Minute;
  Byte Second;
  Byte Hundredths;
  signed char GmtOffset; // min intervals from -48 (West) to +52 (East) recorded.
  bool NotSpecified() const { return Year == 0 && Month == 0 && Day == 0 &&
      Hour == 0 && Minute == 0 && Second == 0 && GmtOffset == 0; }

  bool GetFileTime(FILETIME &ft) const
  {
    UInt64 value;
    bool res = NWindows::NTime::GetSecondsSince1601(Year, Month, Day, Hour, Minute, Second, value);
    if (res)
    {
      value -= (UInt64)((Int64)GmtOffset * 15 * 60);
      value *= 10000000;
    }
    ft.dwLowDateTime = (DWORD)value;
    ft.dwHighDateTime = (DWORD)(value >> 32);
    return res;
  }
};

struct CBootRecordDescriptor
{
  Byte BootSystemId[32];  // a-characters
  Byte BootId[32];        // a-characters
  Byte BootSystemUse[1977];
};

struct CBootValidationEntry
{
  Byte PlatformId;
  Byte Id[24]; // to identify the manufacturer/developer of the CD-ROM.
};

struct CBootInitialEntry
{
  bool Bootable;
  Byte BootMediaType;
  UInt16 LoadSegment;
  /* This is the load segment for the initial boot image. If this
     value is 0 the system will use the traditional segment of 7C0. If this value
     is non-zero the system will use the specified segment. This applies to x86
     architectures only. For "flat" model architectures (such as Motorola) this
     is the address divided by 10. */
  Byte SystemType;    // This must be a copy of byte 5 (System Type) from the
                      // Partition Table found in the boot image.
  UInt16 SectorCount; // This is the number of virtual/emulated sectors the system
                      // will store at Load Segment during the initial boot procedure.
  UInt32 LoadRBA;     // This is the start address of the virtual disk. CD�s use
                      // Relative/Logical block addressing.

  UInt64 GetSize() const
  {
    // if (BootMediaType == NBootMediaType::k1d44Floppy) (1440 << 10);
    return SectorCount * 512;
  }

  AString GetName() const
  {
    AString s = (Bootable ? "Bootable" : "NotBootable");
    s += '_';
    if (BootMediaType < kNumBootMediaTypes)
      s += kMediaTypes[BootMediaType];
    else
    {
      char name[16];
      ConvertUInt32ToString(BootMediaType, name);
      s += name;
    }
    s += ".img";
    return s;
  }
};

struct CVolumeDescriptor
{
  Byte VolFlags;
  Byte SystemId[32]; // a-characters. An identification of a system
                     // which can recognize and act upon the content of the Logical
                     // Sectors with logical Sector Numbers 0 to 15 of the volume.
  Byte VolumeId[32]; // d-characters. An identification of the volume.
  UInt32 VolumeSpaceSize; // the number of Logical Blocks in which the Volume Space of the volume is recorded
  Byte EscapeSequence[32];
  UInt16 VolumeSetSize;
  UInt16 VolumeSequenceNumber; // the ordinal number of the volume in the Volume Set of which the volume is a member.
  UInt16 LogicalBlockSize;
  UInt32 PathTableSize;
  UInt32 LPathTableLocation;
  UInt32 LOptionalPathTableLocation;
  UInt32 MPathTableLocation;
  UInt32 MOptionalPathTableLocation;
  CDirRecord RootDirRecord;
  Byte VolumeSetId[128];
  Byte PublisherId[128];
  Byte DataPreparerId[128];
  Byte ApplicationId[128];
  Byte CopyrightFileId[37];
  Byte AbstractFileId[37];
  Byte BibFileId[37];
  CDateTime CTime;
  CDateTime MTime;
  CDateTime ExpirationTime;
  CDateTime EffectiveTime;
  Byte FileStructureVersion; // = 1;
  Byte ApplicationUse[512];

  bool IsJoliet() const
  {
    if ((VolFlags & 1) != 0)
      return false;
    Byte b = EscapeSequence[2];
    return (EscapeSequence[0] == 0x25 && EscapeSequence[1] == 0x2F &&
      (b == 0x40 || b == 0x43 || b == 0x45));
  }
};

struct CRef
{
  const CDir *Dir;
  UInt32 Index;
  UInt32 NumExtents;
  UInt64 TotalSize;
};

const UInt32 kBlockSize = 1 << 11;

class CInArchive
{
  IInStream *_stream;
  UInt64 _position;

  UInt32 m_BufferPos;
  
  CDir _rootDir;
  bool _bootIsDefined;
  CBootRecordDescriptor _bootDesc;

  void Skip(size_t size);
  void SkipZeros(size_t size);
  Byte ReadByte();
  void ReadBytes(Byte *data, UInt32 size);
  UInt16 ReadUInt16Spec();
  UInt16 ReadUInt16();
  UInt32 ReadUInt32Le();
  UInt32 ReadUInt32Be();
  UInt32 ReadUInt32();
  UInt64 ReadUInt64();
  UInt32 ReadDigits(int numDigits);
  void ReadDateTime(CDateTime &d);
  void ReadRecordingDateTime(CRecordingDateTime &t);
  void ReadDirRecord2(CDirRecord &r, Byte len);
  void ReadDirRecord(CDirRecord &r);

  void ReadBootRecordDescriptor(CBootRecordDescriptor &d);
  void ReadVolumeDescriptor(CVolumeDescriptor &d);

  void SeekToBlock(UInt32 blockIndex);
  void ReadDir(CDir &d, int level);
  void CreateRefs(CDir &d);

  void ReadBootInfo();
  HRESULT Open2();
public:
  HRESULT Open(IInStream *inStream);
  void Clear();

  UInt64 _fileSize;
  UInt64 PhySize;

  CRecordVector<CRef> Refs;
  CObjectVector<CVolumeDescriptor> VolDescs;
  int MainVolDescIndex;
  UInt32 BlockSize;
  CObjectVector<CBootInitialEntry> BootEntries;

  bool IsArc;
  bool UnexpectedEnd;
  bool HeadersError;
  bool IncorrectBigEndian;
  bool TooDeepDirs;
  bool SelfLinkedDirs;
  CRecordVector<UInt32> UniqStartLocations;

  Byte m_Buffer[kBlockSize];

  void UpdatePhySize(UInt32 blockIndex, UInt64 size)
  {
    UInt64 alignedSize = (size + BlockSize - 1) & ~((UInt64)BlockSize - 1);
    UInt64 end = blockIndex * BlockSize + alignedSize;
    if (PhySize < end)
      PhySize = end;
  }

  bool IsJoliet() const { return VolDescs[MainVolDescIndex].IsJoliet(); }

  UInt64 GetBootItemSize(int index) const
  {
    const CBootInitialEntry &be = BootEntries[index];
    UInt64 size = be.GetSize();
    if (be.BootMediaType == NBootMediaType::k1d2Floppy)
      size = (1200 << 10);
    else if (be.BootMediaType == NBootMediaType::k1d44Floppy)
      size = (1440 << 10);
    else if (be.BootMediaType == NBootMediaType::k2d88Floppy)
      size = (2880 << 10);
    UInt64 startPos = (UInt64)be.LoadRBA * BlockSize;
    if (startPos < _fileSize)
    {
      if (_fileSize - startPos < size)
        size = _fileSize - startPos;
    }
    return size;
  }

  bool IsSusp;
  unsigned SuspSkipSize;
};
  
}}
  
#endif