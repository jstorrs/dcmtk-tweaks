#include "dcmtk/tweaks/dcmdata.h"

namespace Tweak {

  OFBool opt_stdin = OFFalse;
  OFBool opt_print = OFTrue;
  OFBool opt_print_empty = OFFalse;
  OFBool opt_print_filenames = OFFalse;
  OFBool opt_print_basenames = OFFalse;
  OFBool opt_print_filename_last = OFFalse;
  OFBool opt_print_known_uid = OFFalse;
  OFBool opt_print_tag_heirarchy = OFTrue;
  OFBool opt_context = OFFalse;
  OFBool opt_tabulate = OFFalse;
  OFBool opt_headers = OFFalse;
  const char* opt_skip_UID = NULL;
  char field_sep[] = "\t";
  
  int contextTagCount = 0;
  const int MAX_CONTEXT_TAG_NAMES = 1024;
  const char *contextTagNames[MAX_CONTEXT_TAG_NAMES];
  const DcmTagKey *contextTagKeys[MAX_CONTEXT_TAG_NAMES];
  std::stringstream contextStream;

  void
  addOptions(OFCommandLine& cmd)
  {
    cmd.addGroup("tweaks:");
      cmd.addSubGroup("output:");
        cmd.addOption("--print-with-filename", "-H",    "print filenames");
        cmd.addOption("--print-with-basename", "-Hb",   "print filenames without directory");
        cmd.addOption("--print-filename-last", "-He",   "print filenames on end");
        cmd.addOption("--print-empty",         "-e",    "print empty elements");
	cmd.addOption("--print-known-uid",     "-k",    "print known UIDs");
	cmd.addOption("--print-all",           "-a",    "print empty elements and known UIDs");
	cmd.addOption("--skip-uid",            "-U", 1, "[U]ID prefix",
		                                        "don't print UIDs that begin with U");
	cmd.addOption("--tabulate",            "-t",    "tabulate without headers");
	cmd.addOption("--tabulate-headers",    "+t",    "tabulate with headers");
	cmd.addOption("--print-context",      "+Pc", 1, "[t]ag: \"gggg,eeee\" or dictionary name", "prefix lines with context of tag\nthis option can be specified multiple times");
  }


  /* See addPrintTagName */
  static OFBool addContextTagName(const char *tagName, OFLogger& dcmdumpLogger)
  {
    if (contextTagCount >= MAX_CONTEXT_TAG_NAMES) {
      OFLOG_WARN(dcmdumpLogger, "too many context tag options (max: " << MAX_CONTEXT_TAG_NAMES << ")");
      return OFFalse;
    }
    
    unsigned int group = 0xffff;
    unsigned int elem = 0xffff;
    if (sscanf(tagName, "%x,%x", &group, &elem) != 2)
    {
      /* it is a name */
      const DcmDataDictionary &globalDataDict = dcmDataDict.rdlock();
      const DcmDictEntry *dicent = globalDataDict.findEntry(tagName);
      if (dicent == NULL) {
	OFLOG_WARN(dcmdumpLogger, "unrecognized tag name: '" << tagName << "'");
	dcmDataDict.rdunlock();
	return OFFalse;
      } else {
	/* note for later */
	contextTagKeys[contextTagCount] = new DcmTagKey(dicent->getKey());
      }
      dcmDataDict.rdunlock();
    } else {
      /* tag name has format xxxx,xxxx */
      /* do not lookup in dictionary, tag could be private */
      contextTagKeys[contextTagCount] = NULL;
    }
    
    size_t buflen = strlen(tagName)+1;
    char *buf = OFstatic_cast(char*, malloc(buflen));
    OFStandard::strlcpy(buf, tagName, buflen);
    contextTagNames[contextTagCount] = buf;
    contextTagCount++;
    return OFTrue;
  }


  
  static OFBool
  configureFromCommandLine(OFCommandLine& cmd, OFConsoleApplication& app, OFLogger& dcmdumpLogger)
  {
    if (cmd.findOption("--print-with-filename")) {
      opt_print_filenames = OFTrue;
      opt_print_basenames = OFFalse;
    }
    if (cmd.findOption("--print-with-basename")) {
      opt_print_filenames = OFTrue;
      opt_print_basenames = OFTrue;
    }
    if (cmd.findOption("--print-filename-last"))
      opt_print_filename_last = OFTrue;
    if (cmd.findOption("--print-all")) {
      opt_print_empty = OFTrue;
      opt_print_known_uid = OFTrue;
    }
    if (cmd.findOption("--print-empty")) {
      opt_print_empty = OFTrue;
    }
    if (cmd.findOption("--print-known-uid")) {
      opt_print_known_uid = OFTrue;
    }
    if (cmd.findOption("--skip-uid")) {
      cmd.getValue(opt_skip_UID);
    }
    if (cmd.findOption("--print-context", 0, OFCommandLine::FOM_FirstFromLeft))
    {
      const char *tagName = NULL;
      do
      {
        app.checkValue(cmd.getValue(tagName));
        if (!addContextTagName(tagName, dcmdumpLogger)) return OFFalse;
      } while (cmd.findOption("--print-context", 0, OFCommandLine::FOM_NextFromLeft));
    }
    if (cmd.findOption("--tabulate")) {
      opt_tabulate = OFTrue;
      opt_headers = OFFalse;
    }
    if (cmd.findOption("--tabulate-headers")) {
      opt_tabulate = OFTrue;
      opt_headers = OFTrue;
    }
    if (!opt_tabulate && contextTagCount > 0) {
      opt_context = OFTrue;
    }
    if (isatty(STDIN_FILENO))
      opt_stdin = OFFalse;
    else
      opt_stdin = OFTrue;
    return OFTrue;
  }

  
  OFBool
  isKnownUID(DcmObject *obj)
  {
    char *stringVal = NULL;
    Uint32 stringLen = 0;
    OFBool retval = OFFalse;

    if (obj->ident() == EVR_UI)
      {
	static_cast<DcmUniqueIdentifier*>(obj)->getString(stringVal,stringLen);
	if ((stringVal != NULL) && (stringLen > 0)) {
	  const char *symbol = dcmFindNameOfUID(stringVal);
	  if ((symbol != NULL) && (strlen(symbol) > 0))
	    retval = OFTrue;
	  if ((opt_skip_UID != NULL) && (strncmp(stringVal, opt_skip_UID, strlen(opt_skip_UID)) == 0))
	    retval = OFTrue;
	}
      }
    return retval;
  }


  void
  DumpTagPath(const DcmStack& stack,
	      STD_NAMESPACE ostream &out)
  {
    if (opt_print_tag_heirarchy)
      for (int i = stack.card()-1; i>=0; i--)
	{
	  DcmObject* obj = stack.elem(i);
	  switch (obj->ident())
	    {
	    case EVR_metainfo:
	    case EVR_dataset:
	    case EVR_item:
	    case EVR_dirRecord:
	      break;
	    default:
	      out << obj->getTag();
	    }
	}
    else
      out << stack.top()->getTag();
  }

  
  DcmObject*
  GetObjectFromStack(const DcmStack& stack)
  {
    DcmObject* obj = stack.top();
    switch (obj->ident())
      {
      case EVR_metainfo:
      case EVR_dataset:
      case EVR_item:
      case EVR_dirRecord:
      case EVR_PixelData:
	return NULL;
      default:
	break;
      }
    return obj;  
  }


  void
  PrintHeader(const char **printTagNames,
	      int printTagCount,
	      STD_NAMESPACE ostream &out)
  {
    if (opt_headers) {
      opt_headers = OFFalse;

      if (opt_print_filenames)
	out << "Filename" << field_sep;

      for (int i = 0; i < printTagCount; i++) {
	out << (i==0 ? "" : field_sep) << printTagNames[i];
      }
      
      if (opt_print_filename_last)
	out << field_sep << "Filename";

      out << OFendl;
    }
  }
  
  void
  PrintFilename(const OFFilename &ifname,
		STD_NAMESPACE ostream &out)
  {
    OFFilename result;

    if (opt_print_filenames) {
      if (opt_print_basenames) {
	out << OFStandard::getFilenameFromPath(result, ifname);
      } else {
	out << ifname;
      }
    }
  }


  OFBool first_tag = OFFalse;

  void
  FileBegin(const OFFilename &ifname,
	    STD_NAMESPACE ostream &out)
  {
    first_tag = OFTrue;
    if (opt_print_filenames) {
      PrintFilename(ifname, out);
      out << field_sep;
    }
  }


  void
  PrintContext(const OFFilename &ifname,
	       STD_NAMESPACE ostream &out)
  {
    if (opt_context) {
      first_tag = OFTrue;
      std::string s = contextStream.str();
      out << s << field_sep;
    }
  }

  void
  FileEnd(const OFFilename &ifname,
	  STD_NAMESPACE ostream &out)
  {
    if (opt_tabulate && opt_print_filename_last) {
      out << field_sep;
      PrintFilename(ifname, out);
    }
    out << OFendl;
  }


  void
  TagBegin(STD_NAMESPACE ostream &out)
  {
    if (opt_tabulate & !first_tag)
      out << field_sep;
    first_tag = OFFalse;
  }


  void
  TagEnd(STD_NAMESPACE ostream &out)
  {
    ;
  }


  void
  PrintTag(DcmObject* obj,
	   STD_NAMESPACE ostream &out,
	   const size_t flags)
  {
    if (obj) {
      std::stringstream ss;
      Tweaks::PF_ValueOnly = OFTrue;    
      obj->print(ss, flags);
      std::string s = ss.str();
      if (opt_tabulate) {
	if ((s.front() == '[') & (s.back() == ']'))
	  s = s.substr(1, s.size()-2);
	if ((s.front() == '"') & (s.back() == '"'))
	  s = s.substr(1, s.size()-2);
	if ((s.front() == '\'') & (s.back() == '\''))
	  s = s.substr(1, s.size()-2);
	if (s == "(no value available)")
	  s = "";
      }
      out << s;
    }
  }


  void
  PrintRow(const DcmStack& stack,
	   DcmObject* obj,
	   const OFFilename &ifname,
	   STD_NAMESPACE ostream &out,
	   const size_t flags)
  {
    if (obj) {
      if (!opt_print_known_uid && isKnownUID(obj))
	return;
      
      if (!opt_print_empty && (obj->getVM() == 0))
	return;

      DcmTag tag = obj->getTag();
      DcmVR vr;
      vr.setVR(obj->ident());
      const char* authority = tag.getPrivateCreator();
      authority = authority ? authority : "DICOM";

      FileBegin(ifname, out);
      PrintContext(ifname, out);
      out << authority << field_sep;
      DumpTagPath(stack,out);
      out << field_sep
	  << vr.getVRName() << field_sep
	  << tag.getTagName() << field_sep
	  << obj->getVM() << field_sep;
      PrintTag(obj, out, flags);
      out << OFendl;
    }
  }


  void
  DumpObject(const DcmStack& stack,
	     const OFFilename &ifname,
	     STD_NAMESPACE ostream &out,
	     const size_t flags)
  {
    Tweaks::PF_ValueOnly = OFTrue;
    DcmObject* obj = GetObjectFromStack(stack);
    if (opt_tabulate) {
      TagBegin(out);
      PrintTag(obj, out, flags);
    } else
      PrintRow(stack, obj, ifname, out, flags);
  }
  

  void
  DumpChildren(DcmObject* obj,
	       const OFFilename &ifname,
	       STD_NAMESPACE ostream &out,
	       const size_t flags)
  {
    DcmStack stack;
    while (obj->nextObject(stack, OFTrue).good()) {
      DumpObject(stack, ifname, out, flags);
    }
  }

  void
  AddContext(const DcmStack& stack,
	     const size_t flags)
  {
    DcmObject* obj = GetObjectFromStack(stack);
    if (obj) {
      opt_tabulate = OFTrue;
      TagBegin(contextStream);
      PrintTag(obj, contextStream, flags);
      TagEnd(contextStream);
      opt_tabulate = OFFalse;
    }
  }
  
  
  void
  LoadContext(DcmObject* dset,
	      OFLogger& dcmdumpLogger,
	      const size_t flags)
  {
    contextStream.str("");
    first_tag = OFTrue;
    
    /* only context specified tags */
    for (int i = 0; i < contextTagCount; i++)
      {
	unsigned int group = 0xffff;
	unsigned int elem = 0xffff;
	DcmTagKey searchKey;
	const char *tagName = contextTagNames[i];
	if (contextTagKeys[i])
	  searchKey = *contextTagKeys[i];
	else if (sscanf(tagName, "%x,%x", &group, &elem) == 2)
	  searchKey.set(OFstatic_cast(Uint16, group), OFstatic_cast(Uint16, elem));
	else {
	  OFLOG_FATAL(dcmdumpLogger, "Internal ERROR in File " << __FILE__ << ", Line "
		      << __LINE__ << OFendl << " -- Named tag inconsistency");
	  abort();
	}
	
	DcmStack stack;
	if (dset->search(searchKey, stack, ESM_fromHere, OFTrue) == EC_Normal)
	  {
	    AddContext(stack, flags);
	  }
      }

    first_tag = OFTrue;
  }


  void
  DumpDataset(DcmObject* dset,
	      const OFFilename &ifname,
	      STD_NAMESPACE ostream &out,
	      const size_t flags = 0)
  {
    Tweaks::PF_ValueOnly = OFTrue;
    DcmFileFormat* dfile = static_cast<DcmFileFormat*>(dset);
    DumpChildren(dfile->getMetaInfo(), ifname, out, flags);
    DumpChildren(dfile->getDataset(), ifname, out, flags);
  }

  
}
