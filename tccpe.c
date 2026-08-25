/*
 *  TCCPE.C - PE file output for the Tiny C Compiler
 *
 *  Copyright (c) 2005-2007 grischka
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tcc.h"

#define PE_MERGE_DATA 1
#define PE_PRINT_SECTIONS 0

#ifndef _WIN32
#define stricmp strcasecmp
#define strnicmp strncasecmp
#include <sys/stat.h> /* chmod() */
#endif

#if defined TCC_TARGET_X86_64
# define REL_TYPE_DIRECT R_X86_64_64
# define R_XXX_THUNKFIX R_X86_64_PC32
# define R_XXX_RELATIVE R_X86_64_RELATIVE
# define R_XXX_FUNCCALL R_X86_64_PLT32
# define RSRC_RELTYPE 3
# define IMAGE_FILE_MACHINE 0x8664
# define CHARACTERISTICS_EXE 0x022F
# define CHARACTERISTICS_DLL 0x222E
# define IMAGE_BASE_EXE 0x00400000
# define IMAGE_BASE_DLL 0x10000000
# define DLLCHARACTERISTICS 0
# define OS_VER 0x0400

#elif defined TCC_TARGET_ARM
# define REL_TYPE_DIRECT R_ARM_ABS32
# define R_XXX_THUNKFIX R_ARM_ABS32
# define R_XXX_RELATIVE R_ARM_RELATIVE
# define R_XXX_FUNCCALL R_ARM_PC24
# define R_XXX_FUNCCALL2 R_ARM_ABS32
# define RSRC_RELTYPE 7 /* ??? (not tested) */
# define IMAGE_FILE_MACHINE 0x01C0
# define CHARACTERISTICS_EXE 0x010F
# define CHARACTERISTICS_DLL 0x230F
# define IMAGE_BASE_EXE 0x00100000
# define IMAGE_BASE_DLL 0x10000000
# define DLLCHARACTERISTICS 0
# define OS_VER 0x0400

#elif defined TCC_TARGET_ARM64
# define REL_TYPE_DIRECT R_AARCH64_ABS64
# define R_XXX_THUNKFIX R_AARCH64_ABS64
# define R_XXX_RELATIVE R_AARCH64_RELATIVE
# define R_XXX_FUNCCALL R_AARCH64_CALL26
# define RSRC_RELTYPE 3
# define IMAGE_FILE_MACHINE 0xAA64
# define CHARACTERISTICS_EXE 0x0022
# define CHARACTERISTICS_DLL 0x2022
# define IMAGE_BASE_EXE 0x140000000ULL
# define IMAGE_BASE_DLL 0x180000000ULL
# define OS_VER 0x0602
# define DLLCHARACTERISTICS 0x8160

#elif defined TCC_TARGET_I386
# define REL_TYPE_DIRECT R_386_32
# define R_XXX_THUNKFIX R_386_32
# define R_XXX_RELATIVE R_386_RELATIVE
# define R_XXX_FUNCCALL R_386_PC32
# define RSRC_RELTYPE 7 /* DIR32NB */
# define IMAGE_FILE_MACHINE 0x014C
# define CHARACTERISTICS_EXE 0x030F
# define CHARACTERISTICS_DLL 0x230E
# define IMAGE_BASE_EXE 0x00400000
# define IMAGE_BASE_DLL 0x10000000
# define OS_VER 0x0400
# define DLLCHARACTERISTICS 0
#endif

#if PTR_SIZE == 8
# define ADDR3264 ULONGLONG
# define PE_MAGIC 0x020B
# define PE_IMAGE_REL IMAGE_REL_BASED_DIR64
#else
# define ADDR3264 DWORD
# define PE_MAGIC 0x010B
# define PE_IMAGE_REL IMAGE_REL_BASED_HIGHLOW
#endif

/* -Wl,--delay-all : size in bytes of the per-DLL "tail merge" stub
   emitted by pe_emit_delay_tailmerge(); must be reserved in
   text_section up front (see struct pe_import_info::delay_tm_offset) */
#if defined(TCC_TARGET_X86_64)
# define DELAY_TAILMERGE_SIZE 119
#elif defined(TCC_TARGET_I386)
# define DELAY_TAILMERGE_SIZE 20
#else
# define DELAY_TAILMERGE_SIZE 0 /* --delay-all unsupported; see pe_build_delay_imports() */
#endif

#ifndef IMAGE_NT_SIGNATURE
/* cross compiler: windows.h was not included */
/* ----------------------------------------------------------- */
/* definitions below are from winnt.h */

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned long long ULONGLONG;
#pragma pack(push, 1)

typedef struct _IMAGE_DOS_HEADER {  /* DOS .EXE header */
    WORD e_magic;         /* Magic number */
    WORD e_cblp;          /* Bytes on last page of file */
    WORD e_cp;            /* Pages in file */
    WORD e_crlc;          /* Relocations */
    WORD e_cparhdr;       /* Size of header in paragraphs */
    WORD e_minalloc;      /* Minimum extra paragraphs needed */
    WORD e_maxalloc;      /* Maximum extra paragraphs needed */
    WORD e_ss;            /* Initial (relative) SS value */
    WORD e_sp;            /* Initial SP value */
    WORD e_csum;          /* Checksum */
    WORD e_ip;            /* Initial IP value */
    WORD e_cs;            /* Initial (relative) CS value */
    WORD e_lfarlc;        /* File address of relocation table */
    WORD e_ovno;          /* Overlay number */
    WORD e_res[4];        /* Reserved words */
    WORD e_oemid;         /* OEM identifier (for e_oeminfo) */
    WORD e_oeminfo;       /* OEM information; e_oemid specific */
    WORD e_res2[10];      /* Reserved words */
    DWORD e_lfanew;        /* File address of new exe header */
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;

#define IMAGE_NT_SIGNATURE  0x00004550  /* PE00 */

typedef struct _IMAGE_FILE_HEADER {
    WORD    Machine;
    WORD    NumberOfSections;
    DWORD   TimeDateStamp;
    DWORD   PointerToSymbolTable;
    DWORD   NumberOfSymbols;
    WORD    SizeOfOptionalHeader;
    WORD    Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;


#define IMAGE_SIZEOF_FILE_HEADER 20

typedef struct _IMAGE_DATA_DIRECTORY {
    DWORD   VirtualAddress;
    DWORD   Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;


typedef struct _IMAGE_OPTIONAL_HEADER {
    /* Standard fields. */
    WORD    Magic;
    BYTE    MajorLinkerVersion;
    BYTE    MinorLinkerVersion;
    DWORD   SizeOfCode;
    DWORD   SizeOfInitializedData;
    DWORD   SizeOfUninitializedData;
    DWORD   AddressOfEntryPoint;
    DWORD   BaseOfCode;
#if PTR_SIZE == 4
    DWORD   BaseOfData;
#endif
    /* NT additional fields. */
    ADDR3264 ImageBase;
    DWORD   SectionAlignment;
    DWORD   FileAlignment;
    WORD    MajorOperatingSystemVersion;
    WORD    MinorOperatingSystemVersion;
    WORD    MajorImageVersion;
    WORD    MinorImageVersion;
    WORD    MajorSubsystemVersion;
    WORD    MinorSubsystemVersion;
    DWORD   Win32VersionValue;
    DWORD   SizeOfImage;
    DWORD   SizeOfHeaders;
    DWORD   CheckSum;
    WORD    Subsystem;
    WORD    DllCharacteristics;
    ADDR3264 SizeOfStackReserve;
    ADDR3264 SizeOfStackCommit;
    ADDR3264 SizeOfHeapReserve;
    ADDR3264 SizeOfHeapCommit;
    DWORD   LoaderFlags;
    DWORD   NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER32, IMAGE_OPTIONAL_HEADER64, IMAGE_OPTIONAL_HEADER;

#define IMAGE_DIRECTORY_ENTRY_EXPORT          0   /* Export Directory */
#define IMAGE_DIRECTORY_ENTRY_IMPORT          1   /* Import Directory */
#define IMAGE_DIRECTORY_ENTRY_RESOURCE        2   /* Resource Directory */
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION       3   /* Exception Directory */
#define IMAGE_DIRECTORY_ENTRY_SECURITY        4   /* Security Directory */
#define IMAGE_DIRECTORY_ENTRY_BASERELOC       5   /* Base Relocation Table */
#define IMAGE_DIRECTORY_ENTRY_DEBUG           6   /* Debug Directory */
/*      IMAGE_DIRECTORY_ENTRY_COPYRIGHT       7      (X86 usage) */
#define IMAGE_DIRECTORY_ENTRY_ARCHITECTURE    7   /* Architecture Specific Data */
#define IMAGE_DIRECTORY_ENTRY_GLOBALPTR       8   /* RVA of GP */
#define IMAGE_DIRECTORY_ENTRY_TLS             9   /* TLS Directory */
#define IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG    10   /* Load Configuration Directory */
#define IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT   11   /* Bound Import Directory in headers */
#define IMAGE_DIRECTORY_ENTRY_IAT            12   /* Import Address Table */
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT   13   /* Delay Load Import Descriptors */
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR 14   /* COM Runtime descriptor */

/* Section header format. */
#define IMAGE_SIZEOF_SHORT_NAME         8

typedef struct _IMAGE_SECTION_HEADER {
    BYTE    Name[IMAGE_SIZEOF_SHORT_NAME];
    union {
            DWORD   PhysicalAddress;
            DWORD   VirtualSize;
    } Misc;
    DWORD   VirtualAddress;
    DWORD   SizeOfRawData;
    DWORD   PointerToRawData;
    DWORD   PointerToRelocations;
    DWORD   PointerToLinenumbers;
    WORD    NumberOfRelocations;
    WORD    NumberOfLinenumbers;
    DWORD   Characteristics;
} IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;

#define IMAGE_SIZEOF_SECTION_HEADER     40

typedef struct _IMAGE_EXPORT_DIRECTORY {
    DWORD Characteristics;
    DWORD TimeDateStamp;
    WORD MajorVersion;
    WORD MinorVersion;
    DWORD Name;
    DWORD Base;
    DWORD NumberOfFunctions;
    DWORD NumberOfNames;
    DWORD AddressOfFunctions;
    DWORD AddressOfNames;
    DWORD AddressOfNameOrdinals;
} IMAGE_EXPORT_DIRECTORY,*PIMAGE_EXPORT_DIRECTORY;

typedef struct _IMAGE_TLS_DIRECTORY {
    ADDR3264 StartAddressOfRawData;
    ADDR3264 EndAddressOfRawData;
    ADDR3264 AddressOfIndex;
    ADDR3264 AddressOfCallBacks;
    DWORD SizeOfZeroFill;
    DWORD Characteristics;
} IMAGE_TLS_DIRECTORY;

typedef struct _IMAGE_IMPORT_DESCRIPTOR {
    union {
        DWORD Characteristics;
        DWORD OriginalFirstThunk;
    };
    DWORD TimeDateStamp;
    DWORD ForwarderChain;
    DWORD Name;
    DWORD FirstThunk;
} IMAGE_IMPORT_DESCRIPTOR;

/* modern (RVA-based) delay-load import descriptor, PE/COFF spec 4.3 */
typedef struct _IMAGE_DELAYLOAD_DESCRIPTOR {
    DWORD Attributes;                    /* bit 0: RvaBased */
    DWORD DllNameRVA;
    DWORD ModuleHandleRVA;
    DWORD ImportAddressTableRVA;
    DWORD ImportNameTableRVA;
    DWORD BoundImportAddressTableRVA;
    DWORD UnloadInformationTableRVA;
    DWORD TimeDateStamp;
} IMAGE_DELAYLOAD_DESCRIPTOR;

typedef struct _IMAGE_BASE_RELOCATION {
    DWORD   VirtualAddress;
    DWORD   SizeOfBlock;
//  WORD    TypeOffset[1];
} IMAGE_BASE_RELOCATION;

#define IMAGE_SIZEOF_BASE_RELOCATION     8

#define IMAGE_REL_BASED_ABSOLUTE         0
#define IMAGE_REL_BASED_HIGH             1
#define IMAGE_REL_BASED_LOW              2
#define IMAGE_REL_BASED_HIGHLOW          3
#define IMAGE_REL_BASED_HIGHADJ          4
#define IMAGE_REL_BASED_MIPS_JMPADDR     5
#define IMAGE_REL_BASED_SECTION          6
#define IMAGE_REL_BASED_REL32            7
#define IMAGE_REL_BASED_DIR64           10

#define IMAGE_SCN_CNT_CODE                  0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA      0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA    0x00000080
#define IMAGE_SCN_MEM_DISCARDABLE           0x02000000
#define IMAGE_SCN_MEM_SHARED                0x10000000
#define IMAGE_SCN_MEM_EXECUTE               0x20000000
#define IMAGE_SCN_MEM_READ                  0x40000000
#define IMAGE_SCN_MEM_WRITE                 0x80000000

#define IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA 0x0020
#define IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE 0x0040
#define IMAGE_DLLCHARACTERISTICS_NX_COMPAT 0x0100
#define IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE 0x8000

#define IMAGE_FILE_RELOCS_STRIPPED 0x0001
#define IMAGE_FILE_DEBUG_STRIPPED 0x0200

#pragma pack(pop)

/* ----------------------------------------------------------- */
#endif /* ndef IMAGE_NT_SIGNATURE */
/* ----------------------------------------------------------- */

#ifndef IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE
  /* allow self-host build with tcc 0.9.27 - doesn't have this in winnt.h */
  #define IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE 0x0040
#endif

#pragma pack(push, 1)
struct pe_header
{
    IMAGE_DOS_HEADER doshdr;
    BYTE dosstub[0x40];
    DWORD nt_sig;
    IMAGE_FILE_HEADER filehdr;
#if PTR_SIZE == 8
    IMAGE_OPTIONAL_HEADER64 opthdr;
#else
#ifdef _WIN64
    IMAGE_OPTIONAL_HEADER32 opthdr;
#else
    IMAGE_OPTIONAL_HEADER opthdr;
#endif
#endif
};

struct pe_reloc_header {
    DWORD offset;
    DWORD size;
};

struct pe_rsrc_header {
    struct _IMAGE_FILE_HEADER filehdr;
    struct _IMAGE_SECTION_HEADER sectionhdr;
};

struct pe_rsrc_reloc {
    DWORD offset;
    DWORD size;
    WORD type;
};
#pragma pack(pop)

/* ------------------------------------------------------------- */
/* internal temporary structures */

enum {
    sec_text = 0,
    sec_rdata ,
    sec_data ,
    sec_bss ,
    sec_idata ,
    sec_pdata ,
    sec_tls ,
    sec_other ,
    sec_rsrc ,
    sec_debug ,
    sec_reloc ,
    sec_last
};

#if 0
static const DWORD pe_sec_flags[] = {
    0x60000020, /* ".text"     , */
    0xC0000040, /* ".data"     , */
    0xC0000080, /* ".bss"      , */
    0x40000040, /* ".idata"    , */
    0x40000040, /* ".pdata"    , */
    0xE0000060, /* < other >   , */
    0x40000040, /* ".rsrc"     , */
    0x42000802, /* ".stab"     , */
    0x42000040, /* ".reloc"    , */
};
#endif

struct section_info {
    int cls;
    char name[32];
    ADDR3264 sh_addr;
    DWORD sh_size;
    DWORD pe_flags;
    Section *sec;
    DWORD data_size;
    IMAGE_SECTION_HEADER ish;
};

struct import_symbol {
    int sym_index;
    int iat_index;
    int thk_offset;
};

struct pe_import_info {
    int dll_index;
    int sym_count;
    struct import_symbol **symbols;
    /* --delay-all only: offset in text_section of this DLL's shared
       "tail merge" stub, reserved up front (like import_symbol's own
       thk_offset) because text_section's size is fixed by the time
       pe_build_delay_imports() runs -- appending to text_section that
       late would land past the section's already-computed extent */
    int delay_tm_offset;
};

struct pe_info {
    TCCState *s1;
    Section *reloc;
    Section *thunk;
    Section *coffsym;
    Section *coffstr;
    const char *filename;
    int type;
    DWORD sizeofheaders;
    ADDR3264 imagebase;
    const char *start_symbol;
    DWORD start_addr;
    DWORD imp_offs;
    DWORD imp_size;
    DWORD iat_offs;
    DWORD iat_size;
    DWORD delay_offs;
    DWORD delay_size;
    DWORD exp_offs;
    DWORD exp_size;
    DWORD tls_dir;
    DWORD tls_data;
    DWORD tls_size;
    int subsystem;
    DWORD section_align;
    DWORD file_align;
    struct section_info **sec_info;
    int sec_count;
    struct pe_import_info **imp_info;
    int imp_count;
    /* function symbols delay-loaded because of --delay-all; kept apart
       from imp_info so that pe_build_imports() (ordinary imports, e.g.
       data symbols that --delay-all can't delay-load) is untouched */
    struct pe_import_info **delay_imp_info;
    int delay_imp_count;
    /* output */
    FILE *op;
    DWORD sum;
    unsigned pos;
};

#define PE_NUL 0
#define PE_DLL 1
#define PE_GUI 2
#define PE_EXE 3
#define PE_RUN 4

/* --------------------------------------------*/

static const char *pe_export_name(TCCState *s1, ElfW(Sym) *sym)
{
    const char *name = (char*)symtab_section->link->data + sym->st_name;
    if (s1->leading_underscore && name[0] == '_' && !(sym->st_other & ST_PE_STDCALL))
        return name + 1;
    return name;
}


static int dynarray_assoc(void **pp, int n, int key)
{
    int i;
    for (i = 0; i < n; ++i, ++pp)
    if (key == **(int **) pp)
        return i;
    return -1;
}

static DWORD umin(DWORD a, DWORD b)
{
    return a < b ? a : b;
}

static DWORD umax(DWORD a, DWORD b)
{
    return a < b ? b : a;
}

static DWORD pe_file_align(struct pe_info *pe, DWORD n)
{
    return (n + (pe->file_align - 1)) & ~(pe->file_align - 1);
}

static ADDR3264 pe_virtual_align(struct pe_info *pe, ADDR3264 n)
{
    return (n + (pe->section_align - 1)) & ~(ADDR3264)(pe->section_align - 1);
}

static void pe_align_section(Section *s, int a)
{
    int i = s->data_offset & (a-1);
    if (i)
        section_ptr_add(s, a - i);
}

static void pe_set_datadir(struct pe_header *hdr, int dir, DWORD addr, DWORD size)
{
    hdr->opthdr.DataDirectory[dir].VirtualAddress = addr;
    hdr->opthdr.DataDirectory[dir].Size = size;
}

static int pe_fwrite(struct pe_info *pe, const void *data, int len)
{
    const WORD *p = data;
    DWORD sum;
    int ret, i;
    pe->pos += (ret = fwrite(data, 1, len, pe->op));
    sum = pe->sum;
    for (i = len; i > 0; i -= 2) {
        sum += (i >= 2) ? *p++ : *(BYTE*)p;
        sum = (sum + (sum >> 16)) & 0xFFFF;
    }
    pe->sum = sum;
    return len == ret ? 0 : -1;
}

static void pe_fpad(struct pe_info *pe, DWORD new_pos)
{
    char buf[256];
    int n, diff = new_pos - pe->pos;
    memset(buf, 0, sizeof buf);
    while (diff > 0) {
        diff -= n = umin(diff, sizeof buf);
        fwrite(buf, n, 1, pe->op);
    }
    pe->pos = new_pos;
}

/*----------------------------------------------------------------------------*/
/* some DWARF support with COFF symbol/string table for gdb */

#pragma pack(push, 1)
struct syment
{
    union {
        char        n_name[8];     /* old COFF version */
        struct {
            int32_t n_zeroes;      /* new == 0 */
            int32_t n_offset;      /* offset into string table */
        };
    };
    int32_t         n_value;        /* value of symbol */
    short           n_scnum;        /* section number */
    unsigned short  n_type;         /* type and derived type */
    char            n_sclass;       /* storage class */
    char            n_numaux;       /* number of aux. entries */
};
#pragma pack(pop)

#define SHF_PRIVATE 0x80000000

static void pe_add_coffsym(struct pe_info *pe)
{
    TCCState *s1 = pe->s1;
    ElfSym *esym;
    struct syment *se;
    int n;

    if (NULL == pe->coffsym) {
        pe->coffsym = new_section(s1, ".coffsym", SHT_PROGBITS, SHF_PRIVATE);
        pe->coffstr = new_section(s1, ".coffstr", SHT_PROGBITS, SHF_PRIVATE);
        section_ptr_add(pe->coffstr, 4); /* coff string table size */
        return;
    }

#if 0
    se = section_ptr_add(pe->coffsym, sizeof *se);
    strcpy(se->n_name, ".file");
    se->n_scnum = -2;
    se->n_sclass = 0x67;
    se->n_numaux = 1;
    se = section_ptr_add(pe->coffsym, sizeof *se);
    strcpy((char*)se, "no-file");
#endif

#if 1
    esym = (ElfSym*)s1->symtab->data;
    for (n = s1->symtab->data_offset / sizeof *esym; ++esym, --n;) {
        int sym_bind = ELFW(ST_BIND)(esym->st_info);
        if (sym_bind == STB_GLOBAL) {
            char *name = esym->st_name + (char*)s1->symtab->link->data;
            int nl = strlen(name);
            addr_t value = esym->st_value;
            int shnum = esym->st_shndx;
            if (shnum != SHN_UNDEF && shnum < s1->nb_sections) {
                Section *s = s1->sections[shnum];
                shnum = s->sh_info;
                value = value - s->sh_addr;
            }
            se = section_ptr_add(pe->coffsym, sizeof *se);
            se->n_value = value;
            se->n_scnum = shnum;
            se->n_sclass = 2; // C_EXT
            if (nl <= 8)
                memcpy(se->n_name, name, nl);
            else
                se->n_offset = put_elf_str(pe->coffstr, name);
        }
    }
#endif
    write32le(pe->coffstr->data, pe->coffstr->data_offset); /* coff string table size */
}

/* Run cv2pdb, available at https://github.com/rainers/cv2pdb.  It reads
   and strips the dwarf info and creates a <exename>.pdb file instead */
static void pe_create_pdb(TCCState *s1, const char *exename)
{
    char buf[300]; int r;
    snprintf(buf, sizeof buf, "cv2pdb.exe \"%s\"", exename);
    r = system(buf);
    strcpy(tcc_fileextension(strcpy(buf, exename)), ".pdb");
    if (r) {
        tcc_error_noabort("could not create '%s'\n(need working cv2pdb from https://github.com/rainers/cv2pdb)", buf);
    } else if (s1->verbose) {
        printf("<- %s\n", buf);
    }
}

/*----------------------------------------------------------------------------*/
static int pe_write(struct pe_info *pe)
{
    static const struct pe_header pe_template = {
    {
    /* IMAGE_DOS_HEADER doshdr */
    0x5A4D, /*WORD e_magic;         Magic number */
    0x0090, /*WORD e_cblp;          Bytes on last page of file */
    0x0003, /*WORD e_cp;            Pages in file */
    0x0000, /*WORD e_crlc;          Relocations */

    0x0004, /*WORD e_cparhdr;       Size of header in paragraphs */
    0x0000, /*WORD e_minalloc;      Minimum extra paragraphs needed */
    0xFFFF, /*WORD e_maxalloc;      Maximum extra paragraphs needed */
    0x0000, /*WORD e_ss;            Initial (relative) SS value */

    0x00B8, /*WORD e_sp;            Initial SP value */
    0x0000, /*WORD e_csum;          Checksum */
    0x0000, /*WORD e_ip;            Initial IP value */
    0x0000, /*WORD e_cs;            Initial (relative) CS value */
    0x0040, /*WORD e_lfarlc;        File address of relocation table */
    0x0000, /*WORD e_ovno;          Overlay number */
    {0,0,0,0}, /*WORD e_res[4];     Reserved words */
    0x0000, /*WORD e_oemid;         OEM identifier (for e_oeminfo) */
    0x0000, /*WORD e_oeminfo;       OEM information; e_oemid specific */
    {0,0,0,0,0,0,0,0,0,0}, /*WORD e_res2[10];      Reserved words */
    0x00000080  /*DWORD   e_lfanew;        File address of new exe header */
    },{
    /* BYTE dosstub[0x40] */
    /* 14 code bytes + "This program cannot be run in DOS mode.\r\r\n$" + 6 * 0x00 */
    0x0e,0x1f,0xba,0x0e,0x00,0xb4,0x09,0xcd,0x21,0xb8,0x01,0x4c,0xcd,0x21,0x54,0x68,
    0x69,0x73,0x20,0x70,0x72,0x6f,0x67,0x72,0x61,0x6d,0x20,0x63,0x61,0x6e,0x6e,0x6f,
    0x74,0x20,0x62,0x65,0x20,0x72,0x75,0x6e,0x20,0x69,0x6e,0x20,0x44,0x4f,0x53,0x20,
    0x6d,0x6f,0x64,0x65,0x2e,0x0d,0x0d,0x0a,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    },
    0x00004550, /* DWORD nt_sig = IMAGE_NT_SIGNATURE */
    {
    /* IMAGE_FILE_HEADER filehdr */
    IMAGE_FILE_MACHINE, /*WORD    Machine; */
    0x0003, /*WORD    NumberOfSections; */
    0x00000000, /*DWORD   TimeDateStamp; */
    0x00000000, /*DWORD   PointerToSymbolTable; */
    0x00000000, /*DWORD   NumberOfSymbols; */
    0x00E0 + (PTR_SIZE-4)*4, /*WORD    SizeOfOptionalHeader; */
    CHARACTERISTICS_EXE, /*WORD    Characteristics; */
    },{
    /* IMAGE_OPTIONAL_HEADER opthdr */
    /* Standard fields. */
    PE_MAGIC, /*WORD    Magic; */
    0x06, /*BYTE    MajorLinkerVersion; */
    0x00, /*BYTE    MinorLinkerVersion; */
    0x00000000, /*DWORD   SizeOfCode; */
    0x00000000, /*DWORD   SizeOfInitializedData; */
    0x00000000, /*DWORD   SizeOfUninitializedData; */
    0x00000000, /*DWORD   AddressOfEntryPoint; */
    0x00000000, /*DWORD   BaseOfCode; */
#if PTR_SIZE == 4
    0x00000000, /*DWORD   BaseOfData; */
#endif
    /* NT additional fields. */
    0x00000000,	/*ADDR3264   ImageBase; */
    0x00001000, /*DWORD   SectionAlignment; */
    0x00000200, /*DWORD   FileAlignment; */
    OS_VER >> 8, /*WORD    MajorOperatingSystemVersion; */
    OS_VER & 255, /*WORD    MinorOperatingSystemVersion; */
    0x0000, /*WORD    MajorImageVersion; */
    0x0000, /*WORD    MinorImageVersion; */
    OS_VER >> 8, /*WORD    MajorSubsystemVersion; */
    OS_VER & 255, /*WORD    MinorSubsystemVersion; */
    0x00000000, /*DWORD   Win32VersionValue; */
    0x00000000, /*DWORD   SizeOfImage; */
    0x00000200, /*DWORD   SizeOfHeaders; */
    0x00000000, /*DWORD   CheckSum; */
    0x0002, /*WORD    Subsystem; */
    DLLCHARACTERISTICS, /*WORD    DllCharacteristics; */
    0x00100000, /*ADDR3264 SizeOfStackReserve; */
    0x00001000, /*ADDR3264 SizeOfStackCommit; */
    0x00100000, /*ADDR3264 SizeOfHeapReserve; */
    0x00001000, /*ADDR3264 SizeOfHeapCommit; */
    0x00000000, /*DWORD   LoaderFlags; */
    0x00000010, /*DWORD   NumberOfRvaAndSizes; */

    /* IMAGE_DATA_DIRECTORY DataDirectory[16]; */
    {{0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
     {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}}
    }};

    struct pe_header pe_header = pe_template;

    int i;
    DWORD file_offset;
    struct section_info *si;
    IMAGE_SECTION_HEADER *psh;
    TCCState *s1 = pe->s1;

    if (s1->do_debug)
        pe_add_coffsym(pe);

    /* pe_file_align()/pe_virtual_align() round with a bit mask, which is
       correct only for a power of two.  Any other value silently produces a
       header advertising an alignment the layout does not obey, so this is
       an error rather than a warning: unlike the sub-page section alignment
       warned about below, such an image is self-contradictory, not merely
       unloadable by a current Windows.  Checked here, before the output file
       is created, so a rejected build leaves no truncated .exe behind; still
       in pe_write() rather than pe_set_options(), which also runs on the
       -run path, where no header is written and the values do not matter.
       tcc's own defaults (0x20/0x200/0x1000) can never trip these. */
    if (pe->file_align & (pe->file_align - 1))
        return tcc_error_noabort("file alignment 0x%x is not a power of two",
            pe->file_align);
    if (pe->section_align & (pe->section_align - 1))
        return tcc_error_noabort("section alignment 0x%x is not a power of two",
            pe->section_align);
    /* PE Format, Optional Header Windows-Specific Fields: SectionAlignment
       "must be greater than or equal to FileAlignment" */
    if (pe->file_align > pe->section_align)
        return tcc_error_noabort("file alignment 0x%x exceeds section"
            " alignment 0x%x", pe->file_align, pe->section_align);

    pe->op = fopen(pe->filename, "wb");
    if (NULL == pe->op)
        return tcc_error_noabort("could not write '%s': %s", pe->filename, strerror(errno));

    pe->sizeofheaders = pe_file_align(pe,
        sizeof (struct pe_header)
        + pe->sec_count * sizeof (IMAGE_SECTION_HEADER)
        );

    file_offset = pe->sizeofheaders;

    if (2 == s1->verbose)
        printf("-------------------------------"
               "\n  virt   file   size  section" "\n");
    for (i = 0; i < pe->sec_count; ++i) {
        DWORD addr, size;
        const char *sh_name;

        si = pe->sec_info[i];
        sh_name = si->name;
        addr = si->sh_addr - pe->imagebase;
        size = si->sh_size;
        psh = &si->ish;

        if (2 == s1->verbose)
            printf("%6x %6x %6x  %s\n",
                (unsigned)addr, (unsigned)file_offset, (unsigned)size, sh_name);

        switch (si->cls) {
            case sec_text:
                if (!pe_header.opthdr.BaseOfCode)
                    pe_header.opthdr.BaseOfCode = addr;
                break;

            case sec_data:
#if PTR_SIZE == 4
                if (!pe_header.opthdr.BaseOfData)
                    pe_header.opthdr.BaseOfData = addr;
#endif
                break;

            case sec_bss:
                break;

            case sec_reloc:
                pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_BASERELOC, addr, size);
                break;

            case sec_rsrc:
                pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_RESOURCE, addr, size);
                break;

            case sec_pdata:
                pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_EXCEPTION, addr, size);
                break;
        }

        if (pe->imp_size) {
            pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_IMPORT,
                pe->imp_offs, pe->imp_size);
            pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_IAT,
                pe->iat_offs, pe->iat_size);
        }
        if (pe->delay_size) {
            pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT,
                pe->delay_offs, pe->delay_size);
        }
        if (pe->exp_size) {
            pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_EXPORT,
                pe->exp_offs, pe->exp_size);
        }
        if (pe->tls_size) {
            pe_set_datadir(&pe_header, IMAGE_DIRECTORY_ENTRY_TLS,
                pe->tls_dir + (pe->thunk->sh_addr - pe->imagebase), pe->tls_size);
        }

        if (pe->coffstr && strlen(sh_name) > 8) {
            /* long section name, for example ".debug_info": the header holds
               "/<offset>" into the coff string table.  Nothing else may be
               copied in first -- Name is a null-padded 8-byte field, and the
               tail of the real name would otherwise survive past the NUL. */
            snprintf((char*)psh->Name, sizeof psh->Name, "/%d",
                put_elf_str(pe->coffstr, sh_name));
        } else {
            memcpy(psh->Name, sh_name, umin(strlen(sh_name), sizeof psh->Name));
        }

        psh->Characteristics = si->pe_flags;
        psh->VirtualAddress = addr;
        psh->Misc.VirtualSize = size;
        pe_header.opthdr.SizeOfImage =
            umax(pe_virtual_align(pe, size + addr), pe_header.opthdr.SizeOfImage);

        /* PE Format, Optional Header Standard Fields: SizeOfUninitializedData
           is "the sum of all such sections if there are multiple BSS
           sections".  Keyed on the flag that is written into this very
           section header, so the two always agree.  A BSS section has no
           file-resident data and so never reaches the SizeOfCode /
           SizeOfInitializedData split below; its size is its virtual size. */
        if (si->pe_flags & IMAGE_SCN_CNT_UNINITIALIZED_DATA)
            pe_header.opthdr.SizeOfUninitializedData += size;

        if (si->data_size) {
            psh->PointerToRawData = file_offset;
            file_offset = pe_file_align(pe, file_offset + si->data_size);
            psh->SizeOfRawData = file_offset - psh->PointerToRawData;
            if (si->cls == sec_text)
                pe_header.opthdr.SizeOfCode += psh->SizeOfRawData;
            else
                pe_header.opthdr.SizeOfInitializedData += psh->SizeOfRawData;
        }
    }

    //pe_header.filehdr.TimeDateStamp = time(NULL);
    pe_header.filehdr.NumberOfSections = pe->sec_count;
    pe_header.opthdr.AddressOfEntryPoint = pe->start_addr;
    pe_header.opthdr.SizeOfHeaders = pe->sizeofheaders;
    pe_header.opthdr.SectionAlignment = pe->section_align;
    pe_header.opthdr.FileAlignment = pe->file_align;
    /* the errors that rule these values out are raised before the
       output file is created, at the top of this function */
    /* only warn when the user asked for this alignment; tcc's own native
       default is unmeasured (see pe_set_options()) */
    if (s1->section_align && pe->section_align < 0x1000)
        tcc_warning("section alignment 0x%x is below the page size;"
            " modern Windows will not load this image", pe->section_align);
    /* same field: FileAlignment "should be a power of 2 between 512 and
       64 K, inclusive", the exception being a sub-page SectionAlignment,
       which FileAlignment must then match.  A warning, not an error: the
       image stays self-consistent, and as above tcc's own native default is
       not warned about -- only a value the user asked for. */
    if (s1->pe_file_align
        && (pe->file_align < 0x200 || pe->file_align > 0x10000)
        && !(pe->section_align < 0x1000 && pe->file_align == pe->section_align))
        tcc_warning("file alignment 0x%x is outside the 512..64K range the"
            " PE format specifies", pe->file_align);
    pe_header.opthdr.ImageBase = pe->imagebase;
    pe_header.opthdr.Subsystem = pe->subsystem;
    pe_header.opthdr.DllCharacteristics = s1->pe_dll_characteristics;
    if (s1->pe_stack_size)
        pe_header.opthdr.SizeOfStackReserve = s1->pe_stack_size;
    if (PE_DLL == pe->type)
        pe_header.filehdr.Characteristics = CHARACTERISTICS_DLL;
    pe_header.filehdr.Characteristics |= s1->pe_characteristics;
    if (pe->reloc)
        pe_header.filehdr.Characteristics &= ~IMAGE_FILE_RELOCS_STRIPPED;

    if (pe->coffsym) {
        pe_add_coffsym(pe);
        pe_header.filehdr.PointerToSymbolTable = file_offset;
        pe_header.filehdr.NumberOfSymbols
            = pe->coffsym->data_offset / sizeof (struct syment);
        /* the image really does carry debugging information now, so it may
           not claim IMAGE_FILE_DEBUG_STRIPPED.  IMAGE_FILE_LOCAL_SYMS_STRIPPED
           stays set and is accurate: pe_add_coffsym() emits STB_GLOBAL
           symbols only. */
        pe_header.filehdr.Characteristics &= ~IMAGE_FILE_DEBUG_STRIPPED;
    }

    pe_fwrite(pe, &pe_header, sizeof pe_header);
    for (i = 0; i < pe->sec_count; ++i)
        pe_fwrite(pe, &pe->sec_info[i]->ish, sizeof(IMAGE_SECTION_HEADER));

    file_offset = pe->sizeofheaders;
    for (i = 0; i < pe->sec_count; ++i) {
        Section *s;
        si = pe->sec_info[i];
        if (!si->data_size)
            continue;
        for (s = si->sec; s; s = s->prev) {
            pe_fpad(pe, file_offset);
            pe_fwrite(pe, s->data, s->data_offset);
            if (s->prev)
                file_offset += s->prev->sh_addr - s->sh_addr;
        }
        file_offset = si->ish.PointerToRawData + si->ish.SizeOfRawData;
        pe_fpad(pe, file_offset);
    }

    if (pe->coffsym) {
        pe_fwrite(pe, pe->coffsym->data, pe->coffsym->data_offset);
        pe_fwrite(pe, pe->coffstr->data, pe->coffstr->data_offset);
        file_offset = pe->pos;
    }

    pe->sum += file_offset;
    fseek(pe->op, offsetof(struct pe_header, opthdr.CheckSum), SEEK_SET);
    pe_fwrite(pe, &pe->sum, sizeof (DWORD));

    fclose (pe->op);
#ifndef _WIN32
    chmod(pe->filename, 0777);
#endif

    if (2 == s1->verbose)
        printf("-------------------------------\n");
    if (s1->verbose)
        printf("<- %s (%u bytes)\n", pe->filename, (unsigned)file_offset);

    if (s1->do_debug & 16)
        pe_create_pdb(s1, pe->filename);
    return 0;
}

/*----------------------------------------------------------------------------*/

static struct import_symbol *pe_add_import(struct pe_info *pe, int sym_index)
{
    int i;
    int dll_index;
    struct pe_import_info *p;
    struct import_symbol *s;
    ElfW(Sym) *isym;

    isym = (ElfW(Sym) *)pe->s1->dynsymtab_section->data + sym_index;
    dll_index = isym->st_size;

    i = dynarray_assoc ((void**)pe->imp_info, pe->imp_count, dll_index);
    if (-1 != i) {
        p = pe->imp_info[i];
        goto found_dll;
    }
    p = tcc_mallocz(sizeof *p);
    p->dll_index = dll_index;
    dynarray_add(&pe->imp_info, &pe->imp_count, p);

found_dll:
    i = dynarray_assoc ((void**)p->symbols, p->sym_count, sym_index);
    if (-1 != i)
        return p->symbols[i];

    s = tcc_mallocz(sizeof *s);
    dynarray_add(&p->symbols, &p->sym_count, s);
    s->sym_index = sym_index;
    return s;
}

/* same as pe_add_import(), but groups into pe->delay_imp_info instead:
   used for --delay-all's function imports, kept apart from ordinary
   (e.g. data) imports of the same DLL */
static struct import_symbol *pe_add_delay_import(struct pe_info *pe, int sym_index)
{
    TCCState *s1 = pe->s1;
    int i;
    int dll_index;
    struct pe_import_info *p;
    struct import_symbol *s;
    ElfW(Sym) *isym;

    isym = (ElfW(Sym) *)pe->s1->dynsymtab_section->data + sym_index;
    dll_index = isym->st_size;

    i = dynarray_assoc ((void**)pe->delay_imp_info, pe->delay_imp_count, dll_index);
    if (-1 != i) {
        p = pe->delay_imp_info[i];
        goto found_dll;
    }
    p = tcc_mallocz(sizeof *p);
    p->dll_index = dll_index;
    dynarray_add(&pe->delay_imp_info, &pe->delay_imp_count, p);
    /* reserve this DLL's tail-merge stub now, while text_section's
       size is still open (see struct pe_import_info::delay_tm_offset) */
    p->delay_tm_offset = text_section->data_offset;
    section_ptr_add(text_section, DELAY_TAILMERGE_SIZE);

found_dll:
    i = dynarray_assoc ((void**)p->symbols, p->sym_count, sym_index);
    if (-1 != i)
        return p->symbols[i];

    s = tcc_mallocz(sizeof *s);
    dynarray_add(&p->symbols, &p->sym_count, s);
    s->sym_index = sym_index;
    return s;
}

static void pe_free_imports(struct pe_info *pe)
{
    int i;
    for (i = 0; i < pe->imp_count; ++i) {
        struct pe_import_info *p = pe->imp_info[i];
        dynarray_reset(&p->symbols, &p->sym_count);
    }
    dynarray_reset(&pe->imp_info, &pe->imp_count);
    for (i = 0; i < pe->delay_imp_count; ++i) {
        struct pe_import_info *p = pe->delay_imp_info[i];
        dynarray_reset(&p->symbols, &p->sym_count);
    }
    dynarray_reset(&pe->delay_imp_info, &pe->delay_imp_count);
}

/*----------------------------------------------------------------------------*/
static void pe_build_imports(struct pe_info *pe)
{
    int thk_ptr, ent_ptr, dll_ptr, sym_cnt, i;
    DWORD rva_base = pe->thunk->sh_addr - pe->imagebase;
    int ndlls = pe->imp_count;
    TCCState *s1 = pe->s1;

    for (sym_cnt = i = 0; i < ndlls; ++i)
        sym_cnt += pe->imp_info[i]->sym_count;

    if (0 == sym_cnt)
        return;

    pe_align_section(pe->thunk, 16);
    pe->imp_size = (ndlls + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR);
    pe->iat_size = (sym_cnt + ndlls) * sizeof(ADDR3264);
    dll_ptr = pe->thunk->data_offset;
    thk_ptr = dll_ptr + pe->imp_size;
    ent_ptr = thk_ptr + pe->iat_size;
    pe->imp_offs = dll_ptr + rva_base;
    pe->iat_offs = thk_ptr + rva_base;
    section_ptr_add(pe->thunk, pe->imp_size + 2*pe->iat_size);

    for (i = 0; i < pe->imp_count; ++i) {
        IMAGE_IMPORT_DESCRIPTOR *hdr;
        int k, n, dllindex;
        ADDR3264 v;
        struct pe_import_info *p = pe->imp_info[i];
        const char *name;
        DLLReference *dllref;

        dllindex = p->dll_index;
        if (dllindex)
            name = tcc_basename((dllref = s1->loaded_dlls[dllindex-1])->name);
        else
            name = "", dllref = NULL;

        /* put the dll name into the import header */
        v = put_elf_str(pe->thunk, name);
        hdr = (IMAGE_IMPORT_DESCRIPTOR*)(pe->thunk->data + dll_ptr);
        hdr->FirstThunk = thk_ptr + rva_base;
        hdr->OriginalFirstThunk = ent_ptr + rva_base;
        hdr->Name = v + rva_base;

        for (k = 0, n = p->sym_count; k <= n; ++k) {
            if (k < n) {
                int iat_index = p->symbols[k]->iat_index;
                int sym_index = p->symbols[k]->sym_index;
                ElfW(Sym) *imp_sym = (ElfW(Sym) *)s1->dynsymtab_section->data + sym_index;
                const char *name = (char*)s1->dynsymtab_section->link->data + imp_sym->st_name;
                int ordinal;

                /* patch symbol (and possibly its underscored alias) */
                do {
                    ElfW(Sym) *esym = (ElfW(Sym) *)symtab_section->data + iat_index;
                    iat_index = esym->st_value;
                    esym->st_value = thk_ptr;
                    esym->st_shndx = pe->thunk->sh_num;
                } while (iat_index);

                if (dllref)
                    v = 0, ordinal = imp_sym->st_value; /* ordinal from pe_load_def */
                else
                    ordinal = 0, v = imp_sym->st_value; /* address from tcc_add_symbol() */

#ifdef TCC_IS_NATIVE
                if (pe->type == PE_RUN) {
                    if (dllref) {
                        if ( !dllref->handle )
                            dllref->handle = LoadLibraryA(dllref->name);
                        v = (ADDR3264)GetProcAddress(dllref->handle, ordinal?(char*)0+ordinal:name);
                    }
                    if (!v)
                        tcc_error_noabort("could not resolve symbol '%s'", name);
                } else
#endif
                if (ordinal) {
                    v = ordinal | (ADDR3264)1 << (sizeof(ADDR3264)*8 - 1);
                } else {
                    v = pe->thunk->data_offset + rva_base;
                    section_ptr_add(pe->thunk, sizeof(WORD)); /* hint, not used */
                    put_elf_str(pe->thunk, name);
                }

            } else {
                v = 0; /* last entry is zero */
            }

            *(ADDR3264*)(pe->thunk->data+thk_ptr) =
            *(ADDR3264*)(pe->thunk->data+ent_ptr) = v;
            thk_ptr += sizeof (ADDR3264);
            ent_ptr += sizeof (ADDR3264);
        }
        dll_ptr += sizeof(IMAGE_IMPORT_DESCRIPTOR);
    }
}

/* ------------------------------------------------------------- */
#if defined(TCC_TARGET_X86_64) || defined(TCC_TARGET_I386)

/* Emit the per-DLL "tail merge" stub that every per-function delay-load
   thunk of that DLL jumps into.  It preserves the registers a call may
   be passing arguments in, calls __delayLoadHelper2(desc, ppfnIATEntry)
   -- with 'ppfnIATEntry' (the address of the delay import address-table
   slot the caller's thunk is for) having been placed in RAX/EAX by the
   thunk -- restores the registers, and jumps to the address the helper
   returned.  This is the same split MSVC and lld-link generate for
   /DELAYLOAD (see PE/COFF spec 4.3 and lld/COFF/DLL.cpp), except that
   __delayLoadHelper2 here uses the plain (non-decorated) C calling
   convention on i386 too, instead of MSVC's __stdcall there: the
   tail-merge stub cleans the two arguments off the stack itself. */
static int pe_emit_delay_tailmerge(struct pe_info *pe, int off, int desc_sym, int helper_sym)
{
    TCCState *s1 = pe->s1;
    unsigned char *p;

#if defined(TCC_TARGET_X86_64)
    static const unsigned char code[] = {
        0x48, 0x89, 0x4C, 0x24, 0x08,       /* mov [rsp+8],  rcx */
        0x48, 0x89, 0x54, 0x24, 0x10,       /* mov [rsp+10], rdx */
        0x4C, 0x89, 0x44, 0x24, 0x18,       /* mov [rsp+18], r8  */
        0x4C, 0x89, 0x4C, 0x24, 0x20,       /* mov [rsp+20], r9  */
        0x48, 0x83, 0xEC, 0x68,             /* sub rsp, 0x68 */
        0x66, 0x0F, 0x7F, 0x44, 0x24, 0x20, /* movdqa [rsp+20], xmm0 */
        0x66, 0x0F, 0x7F, 0x4C, 0x24, 0x30, /* movdqa [rsp+30], xmm1 */
        0x66, 0x0F, 0x7F, 0x54, 0x24, 0x40, /* movdqa [rsp+40], xmm2 */
        0x66, 0x0F, 0x7F, 0x5C, 0x24, 0x50, /* movdqa [rsp+50], xmm3 */
        0x48, 0x8B, 0xD0,                   /* mov rdx, rax (ppfnIATEntry) */
        0x48, 0x8D, 0x0D, 0, 0, 0, 0,       /* lea rcx, [rip+desc] */
        0xE8, 0, 0, 0, 0,                   /* call __delayLoadHelper2 */
        0x66, 0x0F, 0x6F, 0x44, 0x24, 0x20, /* movdqa xmm0, [rsp+20] */
        0x66, 0x0F, 0x6F, 0x4C, 0x24, 0x30, /* movdqa xmm1, [rsp+30] */
        0x66, 0x0F, 0x6F, 0x54, 0x24, 0x40, /* movdqa xmm2, [rsp+40] */
        0x66, 0x0F, 0x6F, 0x5C, 0x24, 0x50, /* movdqa xmm3, [rsp+50] */
        0x48, 0x8B, 0x4C, 0x24, 0x70,       /* mov rcx, [rsp+70] */
        0x48, 0x8B, 0x54, 0x24, 0x78,       /* mov rdx, [rsp+78] */
        0x4C, 0x8B, 0x84, 0x24, 0x80, 0, 0, 0, /* mov r8, [rsp+80] */
        0x4C, 0x8B, 0x8C, 0x24, 0x88, 0, 0, 0, /* mov r9, [rsp+88] */
        0x48, 0x83, 0xC4, 0x68,             /* add rsp, 0x68 */
        0xFF, 0xE0,                         /* jmp rax */
    };
    p = text_section->data + off;
    memcpy(p, code, sizeof code);
    /* lea rcx, [rip+disp] -> descriptor; disp field at offset 54 */
    write32le(p + 54, (DWORD)-4);
    put_elf_reloc(symtab_section, text_section, off + 54, R_XXX_THUNKFIX, desc_sym);
    /* call __delayLoadHelper2; disp field at offset 59 */
    write32le(p + 59, (DWORD)-4);
    put_elf_reloc(symtab_section, text_section, off + 59, R_XXX_FUNCCALL, helper_sym);
#else /* TCC_TARGET_I386, cdecl variant of MSVC's tailMergeX86 */
    static const unsigned char code[] = {
        0x51,             /* push ecx */
        0x52,             /* push edx */
        0x50,             /* push eax (ppfnIATEntry) */
        0x68, 0, 0, 0, 0, /* push offset descriptor */
        0xE8, 0, 0, 0, 0, /* call __delayLoadHelper2 */
        0x83, 0xC4, 0x08, /* add esp, 8 (cdecl: caller cleans up) */
        0x5A,             /* pop edx */
        0x59,             /* pop ecx */
        0xFF, 0xE0,       /* jmp eax */
    };
    p = text_section->data + off;
    memcpy(p, code, sizeof code);
    /* push offset descriptor: absolute VA, needs a base relocation */
    put_elf_reloc(symtab_section, text_section, off + 4, R_XXX_THUNKFIX, desc_sym);
    /* call __delayLoadHelper2 */
    write32le(p + 9, (DWORD)-4);
    put_elf_reloc(symtab_section, text_section, off + 9, R_XXX_FUNCCALL, helper_sym);
#endif
    return off;
}

/* Emit the per-function delay-load thunk at the space reserved earlier
   in pe_check_symbols(): it loads the address of this function's delay
   import address-table slot and jumps to the shared tail-merge stub. */
static void pe_emit_delay_thunk(struct pe_info *pe, int thk_off, int slot_sym, int tm_off)
{
    TCCState *s1 = pe->s1;
    unsigned char *p = text_section->data + thk_off;

#if defined(TCC_TARGET_X86_64)
    p[0] = 0x48, p[1] = 0x8D, p[2] = 0x05; /* lea rax, [rip+disp] */
    write32le(p + 3, (DWORD)-4);
    put_elf_reloc(symtab_section, text_section, thk_off + 3, R_XXX_THUNKFIX, slot_sym);
    p[7] = 0xE9; /* jmp rel32 */
    write32le(p + 8, tm_off - (thk_off + 12));
#else /* TCC_TARGET_I386 */
    p[0] = 0xB8; /* mov eax, imm32 */
    put_elf_reloc(symtab_section, text_section, thk_off + 1, R_XXX_THUNKFIX, slot_sym);
    p[5] = 0xE9; /* jmp rel32 */
    write32le(p + 6, tm_off - (thk_off + 10));
#endif
}

/* -Wl,--delay-all : build delay-load import descriptors (data
   directory 13) for every imported DLL instead of ordinary ones, and
   the machine-code thunks that make delay loading actually work.  See
   pe_build_imports() above for the parallel, much simpler, ordinary
   case. */
static void pe_build_delay_imports(struct pe_info *pe)
{
    int i, k, n, sym_cnt, ndlls;
    DWORD rva_base = pe->thunk->sh_addr - pe->imagebase;
    TCCState *s1 = pe->s1;
    int helper_sym, dir_off;

    ndlls = pe->delay_imp_count;
    for (sym_cnt = i = 0; i < ndlls; ++i)
        sym_cnt += pe->delay_imp_info[i]->sym_count;
    if (0 == sym_cnt)
        return;

    helper_sym = find_elf_sym(symtab_section, "__delayLoadHelper2");
    if (!helper_sym)
        helper_sym = put_elf_sym(symtab_section, 0, 0,
            ELFW(ST_INFO)(STB_GLOBAL, STT_FUNC), 0, SHN_UNDEF,
            "__delayLoadHelper2");

    pe_align_section(pe->thunk, 16);
    pe_align_section(data_section, PTR_SIZE);

    dir_off = pe->thunk->data_offset;
    section_ptr_add(pe->thunk, (ndlls + 1) * sizeof(IMAGE_DELAYLOAD_DESCRIPTOR));
    pe->delay_offs = dir_off + rva_base;
    pe->delay_size = (ndlls + 1) * sizeof(IMAGE_DELAYLOAD_DESCRIPTOR);
    /* the whole array, including the null terminator entry, starts zeroed */

    for (i = 0; i < ndlls; ++i) {
        struct pe_import_info *p = pe->delay_imp_info[i];
        int dllindex = p->dll_index;
        const char *name = dllindex
            ? tcc_basename(s1->loaded_dlls[dllindex-1]->name) : "";
        DWORD name_rva, nt_rva;
        int mh_off, mh_sym, at_off, at_sym, nt_off;
        int tm_off, desc_sym;
        IMAGE_DELAYLOAD_DESCRIPTOR *d;
        DWORD desc_off = dir_off + i * sizeof(IMAGE_DELAYLOAD_DESCRIPTOR);

        name_rva = put_elf_str(pe->thunk, name) + rva_base;

        /* per-DLL module handle cache: one writable zeroed pointer slot */
        pe_align_section(data_section, PTR_SIZE);
        mh_off = data_section->data_offset;
        section_ptr_add(data_section, PTR_SIZE);
        mh_sym = put_elf_sym(symtab_section, mh_off, PTR_SIZE,
            ELFW(ST_INFO)(STB_LOCAL, STT_OBJECT), 0, data_section->sh_num, NULL);

        /* delay import address table: sym_count slots + null terminator,
           writable (the helper patches these after first resolving) */
        at_off = data_section->data_offset;
        section_ptr_add(data_section, (p->sym_count + 1) * PTR_SIZE);
        at_sym = put_elf_sym(symtab_section, at_off, 0,
            ELFW(ST_INFO)(STB_LOCAL, STT_OBJECT), 0, data_section->sh_num, NULL);

        /* delay import name table: sym_count RVAs + null terminator */
        nt_off = pe->thunk->data_offset;
        nt_rva = nt_off + rva_base;
        section_ptr_add(pe->thunk, (p->sym_count + 1) * sizeof(DWORD));

        desc_sym = put_elf_sym(symtab_section, desc_off,
            sizeof(IMAGE_DELAYLOAD_DESCRIPTOR),
            ELFW(ST_INFO)(STB_LOCAL, STT_OBJECT), 0, pe->thunk->sh_num, NULL);

        d = (IMAGE_DELAYLOAD_DESCRIPTOR*)(pe->thunk->data + desc_off);
        d->Attributes = 1; /* RvaBased */
        d->DllNameRVA = name_rva;
        d->ImportNameTableRVA = nt_rva;
        /* ModuleHandleRVA / ImportAddressTableRVA point into data_section,
           whose final address isn't known yet: fill them in via a
           relocation instead of computing the RVA by hand */
        put_elf_reloc(symtab_section, pe->thunk,
            desc_off + offsetof(IMAGE_DELAYLOAD_DESCRIPTOR, ModuleHandleRVA),
            R_XXX_RELATIVE, mh_sym);
        put_elf_reloc(symtab_section, pe->thunk,
            desc_off + offsetof(IMAGE_DELAYLOAD_DESCRIPTOR, ImportAddressTableRVA),
            R_XXX_RELATIVE, at_sym);

        tm_off = pe_emit_delay_tailmerge(pe, p->delay_tm_offset, desc_sym, helper_sym);

        for (k = 0, n = p->sym_count; k < n; ++k) {
            struct import_symbol *isym = p->symbols[k];
            int slot_off = at_off + k * PTR_SIZE;
            int slot_sym, thunk_sym;
            ElfW(Sym) *imp_sym = (ElfW(Sym)*)s1->dynsymtab_section->data + isym->sym_index;
            const char *fname = (char*)s1->dynsymtab_section->link->data + imp_sym->st_name;
            DWORD hint_rva = pe->thunk->data_offset + rva_base;

            section_ptr_add(pe->thunk, sizeof(WORD)); /* hint, not used */
            put_elf_str(pe->thunk, fname);
            *(DWORD*)(pe->thunk->data + nt_off + k * sizeof(DWORD)) = hint_rva;

            slot_sym = put_elf_sym(symtab_section, slot_off, PTR_SIZE,
                ELFW(ST_INFO)(STB_LOCAL, STT_OBJECT), 0, data_section->sh_num, NULL);
            thunk_sym = put_elf_sym(symtab_section, isym->thk_offset, 0,
                ELFW(ST_INFO)(STB_LOCAL, STT_FUNC), 0, text_section->sh_num, NULL);

            /* the address-table slot initially holds the VA of this
               function's thunk (the helper overwrites it with the real
               function's VA on first call); it's an absolute pointer,
               so it needs a base relocation */
            put_elf_reloc(symtab_section, data_section, slot_off,
                REL_TYPE_DIRECT, thunk_sym);

            pe_emit_delay_thunk(pe, isym->thk_offset, slot_sym, tm_off);
        }
        /* the address/name table null terminators are already zero */
    }
}

#else /* unsupported target: keep the flag harmless rather than a link error */
static void pe_build_delay_imports(struct pe_info *pe)
{
    TCCState *s1 = pe->s1;
    tcc_error_noabort("--delay-all is only supported for the i386 and x86_64 targets");
}
#endif

/* ------------------------------------------------------------- */

struct pe_sort_sym
{
    int index;
    const char *name;
};

static int sym_cmp(const void *va, const void *vb)
{
    const char *ca = (*(struct pe_sort_sym**)va)->name;
    const char *cb = (*(struct pe_sort_sym**)vb)->name;
    return strcmp(ca, cb);
}

static void pe_build_exports(struct pe_info *pe)
{
    ElfW(Sym) *sym;
    int sym_index, sym_end;
    DWORD rva_base, base_o, func_o, name_o, ord_o, str_o;
    IMAGE_EXPORT_DIRECTORY *hdr;
    int sym_count, ord;
    struct pe_sort_sym **sorted, *p;
    TCCState *s1 = pe->s1;

    FILE *op;
    char buf[260];
    const char *dllname;
    const char *name;

    rva_base = pe->thunk->sh_addr - pe->imagebase;
    sym_count = 0, sorted = NULL, op = NULL;

    sym_end = symtab_section->data_offset / sizeof(ElfW(Sym));
    for (sym_index = 1; sym_index < sym_end; ++sym_index) {
        sym = (ElfW(Sym)*)symtab_section->data + sym_index;
        name = pe_export_name(s1, sym);
        if (sym->st_other & ST_PE_EXPORT) {
            p = tcc_malloc(sizeof *p);
            p->index = sym_index;
            p->name = name;
            dynarray_add(&sorted, &sym_count, p);
        }
#if 0
        if (sym->st_other & ST_PE_EXPORT)
            printf("export: %s\n", name);
        if (sym->st_other & ST_PE_STDCALL)
            printf("stdcall: %s\n", name);
#endif
    }

    if (0 == sym_count)
        return;

    qsort (sorted, sym_count, sizeof *sorted, sym_cmp);

    pe_align_section(pe->thunk, 16);
    dllname = tcc_basename(pe->filename);

    base_o = pe->thunk->data_offset;
    func_o = base_o + sizeof(IMAGE_EXPORT_DIRECTORY);
    name_o = func_o + sym_count * sizeof (DWORD);
    ord_o = name_o + sym_count * sizeof (DWORD);
    str_o = ord_o + sym_count * sizeof(WORD);

    hdr = section_ptr_add(pe->thunk, str_o - base_o);
    hdr->Characteristics        = 0;
    hdr->Base                   = 1;
    hdr->NumberOfFunctions      = sym_count;
    hdr->NumberOfNames          = sym_count;
    hdr->AddressOfFunctions     = func_o + rva_base;
    hdr->AddressOfNames         = name_o + rva_base;
    hdr->AddressOfNameOrdinals  = ord_o + rva_base;
    hdr->Name                   = str_o + rva_base;
    put_elf_str(pe->thunk, dllname);

#if 1
    /* automatically write exports to <output-filename>.def */
    pstrcpy(buf, sizeof buf, pe->filename);
    strcpy(tcc_fileextension(buf), ".def");
    op = fopen(buf, "wb");
    if (NULL == op) {
        tcc_error_noabort("could not create '%s': %s", buf, strerror(errno));
    } else {
        fprintf(op, "LIBRARY %s\n\nEXPORTS\n", dllname);
        if (s1->verbose)
            printf("<- %s (%d symbol%s)\n", buf, sym_count, &"s"[sym_count < 2]);
    }
#endif

    for (ord = 0; ord < sym_count; ++ord)
    {
        p = sorted[ord], sym_index = p->index, name = p->name;
        /* insert actual address later in relocate_sections() */
        put_elf_reloc(symtab_section, pe->thunk,
            func_o, R_XXX_RELATIVE, sym_index);
        *(DWORD*)(pe->thunk->data + name_o)
            = pe->thunk->data_offset + rva_base;
        *(WORD*)(pe->thunk->data + ord_o)
            = ord;
        put_elf_str(pe->thunk, name);
        func_o += sizeof (DWORD);
        name_o += sizeof (DWORD);
        ord_o += sizeof (WORD);
        if (op)
            fprintf(op, "%s\n", name);
    }

    pe->exp_offs = base_o + rva_base;
    pe->exp_size = pe->thunk->data_offset - base_o;
    dynarray_reset(&sorted, &sym_count);
    if (op)
        fclose(op);
}

/* ------------------------------------------------------------- */
static void pe_build_reloc (struct pe_info *pe)
{
    DWORD offset, block_ptr, sh_addr, addr;
    int count, i;
    ElfW_Rel *rel, *rel_end;
    Section *s = NULL, *sr;
    struct pe_reloc_header *hdr;
    TCCState *s1 = pe->s1;
    int dwarf = 0, n;

    sh_addr = offset = block_ptr = count = i = 0;
    rel = rel_end = NULL;

    for(;;) {
        if (rel < rel_end) {
            int type = ELFW(R_TYPE)(rel->r_info);
            addr = rel->r_offset + sh_addr;
            ++ rel;
            if (type != REL_TYPE_DIRECT)
                continue;
            if (dwarf) { /* don't runtime-relocate dwarf-to-dwarf */
                n = ((ElfSym *)s1->symtab->data + ELFW(R_SYM)(rel[-1].r_info))->st_shndx;
                if (n >= s1->dwlo && n < s1->dwhi)
                    continue;
            }
            if (count == 0) { /* new block */
                block_ptr = pe->reloc->data_offset;
                section_ptr_add(pe->reloc, sizeof(struct pe_reloc_header));
                offset = addr & 0xFFFFFFFF<<12;
            }
            if ((addr -= offset)  < (1<<12)) { /* one block spans 4k addresses */
                WORD *wp = section_ptr_add(pe->reloc, sizeof (WORD));
                *wp = addr | PE_IMAGE_REL<<12;
                ++count;
                continue;
            }
            -- rel;

        } else if (s) {
            sr = s->reloc;
            if (sr) {
                rel = (ElfW_Rel *)sr->data;
                rel_end = (ElfW_Rel *)(sr->data + sr->data_offset);
                sh_addr = s->sh_addr;
                dwarf = s->sh_num >= s1->dwlo && s->sh_num < s1->dwhi;
            }
            s = s->prev;
            continue;

        } else if (i < pe->sec_count) {
            s = pe->sec_info[i]->sec, ++i;
            continue;

        } else if (!count)
            break;

        /* fill the last block and ready for a new one */
        if (count & 1) /* align for DWORDS */
            section_ptr_add(pe->reloc, sizeof(WORD)), ++count;
        hdr = (struct pe_reloc_header *)(pe->reloc->data + block_ptr);
        hdr -> offset = offset - pe->imagebase;
        hdr -> size = count * sizeof(WORD) + sizeof(struct pe_reloc_header);
        count = 0;
    }
}

/* ------------------------------------------------------------- */
static void pe_build_tls(struct pe_info *pe, Section *s)
{
    TCCState *s1 = pe->s1;
    IMAGE_TLS_DIRECTORY *d;
    int c, n;

    if (0 == s) {
        pe->tls_dir = section_add(pe->thunk, pe->tls_size, 16);
        pe->tls_data = section_add(data_section, PTR_SIZE * (1+3), 16);
        /* put relocations on entries */
        c = put_elf_sym(symtab_section, 0, 0, 0, 0, data_section->sh_num, 0);
        for (n = 0; n < 4; ++n)
            put_elf_reloc(symtab_section, pe->thunk, pe->tls_dir + PTR_SIZE*n, REL_TYPE_DIRECT, c);
        /* for generators */
        set_elf_sym(symtab_section, pe->tls_data, PTR_SIZE * 4,
                ELFW(ST_INFO)(STB_GLOBAL, STT_OBJECT),
                0, data_section->sh_num, "__tls_index");
        return;
    }
    if (0 == s1->tls_start)
        s1->tls_start = s->sh_addr;
    d = (void*)(pe->thunk->data + pe->tls_dir);
    d->StartAddressOfRawData = s1->tls_start - data_section->sh_addr;
    d->EndAddressOfRawData = s->sh_addr + s->data_offset - data_section->sh_addr;
    d->AddressOfIndex = pe->tls_data;
    d->AddressOfCallBacks = pe->tls_data + PTR_SIZE;
    d->SizeOfZeroFill = 0;
    d->Characteristics = 0;
    /* to reuse logic from linux in xxx-link.c */
    s1->tls_end = s1->tls_start;
}

/* ------------------------------------------------------------- */
static int pe_section_class(Section *s)
{
    int type, flags;
    const char *name;
    type = s->sh_type;
    flags = s->sh_flags;
    name = s->name;

    if (0 == memcmp(name, ".stab", 5) || 0 == memcmp(name, ".debug_", 7)) {
        return sec_debug;
    } else if (flags & SHF_ALLOC) {
        if (flags & SHF_TLS)
            return sec_tls;
        if (type == SHT_PROGBITS
         || type == SHT_INIT_ARRAY
         || type == SHT_FINI_ARRAY) {
            if (flags & SHF_EXECINSTR)
                return sec_text;
            if (flags & SHF_WRITE)
                return sec_data;
            if (0 == strcmp(name, ".rsrc"))
                return sec_rsrc;
            if (0 == strcmp(name, ".iedat"))
                return sec_idata;
            if (0 == strcmp(name, ".pdata"))
                return sec_pdata;
            return sec_rdata;
        } else if (type == SHT_NOBITS) {
            return sec_bss;
        }
        return sec_other;
    } else {
        if (0 == strcmp(name, ".reloc"))
            return sec_reloc;
    }
    return sec_last;
}

static int pe_assign_addresses (struct pe_info *pe)
{
    int i, k, n, c, nbs;
    ADDR3264 addr;
    int *sec_order, *sec_cls;
    struct section_info *si;
    Section *s;
    TCCState *s1 = pe->s1;

    if (PE_DLL == pe->type
        || (s1->pe_dll_characteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE))
        pe->reloc = new_section(s1, ".reloc", SHT_PROGBITS, 0);
    //pe->thunk = new_section(s1, ".iedat", SHT_PROGBITS, SHF_ALLOC);

    nbs = s1->nb_sections;
    sec_order = tcc_mallocz(2 * sizeof (int) * nbs);
    sec_cls = sec_order + nbs;
    for (i = 1; i < nbs; ++i) {
        s = s1->sections[i];
        k = pe_section_class(s);
        for (n = i; n > 1 && k < (c = sec_cls[n - 1]); --n)
            sec_cls[n] = c, sec_order[n] = sec_order[n - 1];
        sec_cls[n] = k, sec_order[n] = i;
        if (k == sec_tls)
            pe->tls_size = sizeof (IMAGE_TLS_DIRECTORY);
    }
    si = NULL;
    addr = pe->imagebase + 1;

    for (i = 1; (c = sec_cls[i]) < sec_last; ++i) {
        s = s1->sections[sec_order[i]];

        if (PE_MERGE_DATA && c == sec_bss)
            c = sec_data;

        if (si && c == si->cls && c != sec_debug) {
            /* merge with previous section */
            s->sh_addr = addr = ((addr - 1) | (16 - 1)) + 1;
        } else {
            si = NULL;
            s->sh_addr = addr = pe_virtual_align(pe, addr);
        }

        if (NULL == pe->thunk
            && c == (data_section == rodata_section ? sec_data : sec_rdata))
            pe->thunk = s;

        if (s == pe->thunk) {
            /* ordinary imports (always; e.g. data symbols that
               --delay-all can't delay-load) and delay imports (only
               those pe_check_symbols routed to delay_imp_info) */
            pe_build_imports(pe);
            pe_build_delay_imports(pe);
            pe_build_exports(pe);
            if (pe->tls_size)
                pe_build_tls(pe, NULL);
        }

        if (s == pe->reloc)
            pe_build_reloc (pe);

        if (0 == s->data_offset)
            continue;

        if (si)
            goto add_section;

        si = tcc_mallocz(sizeof *si);
        dynarray_add(&pe->sec_info, &pe->sec_count, si);

        strcpy(si->name, s->name);
        si->cls = c;
        si->sh_addr = addr;

        si->pe_flags = IMAGE_SCN_MEM_READ;
        if (s->sh_flags & SHF_EXECINSTR)
            si->pe_flags |= IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE;
        else if (s->sh_type == SHT_NOBITS && !(s->sh_flags & SHF_TLS))
            si->pe_flags |= IMAGE_SCN_CNT_UNINITIALIZED_DATA;
        else
            si->pe_flags |= IMAGE_SCN_CNT_INITIALIZED_DATA;
        if (s->sh_flags & SHF_WRITE)
            si->pe_flags |= IMAGE_SCN_MEM_WRITE;
        if (0 == (s->sh_flags & SHF_ALLOC))
            si->pe_flags |= IMAGE_SCN_MEM_DISCARDABLE;

add_section:
        s->sh_info = pe->sec_count; /* section number for coff syms */
        addr += s->data_offset;
        si->sh_size = addr - si->sh_addr;
        if (s->sh_type != SHT_NOBITS) {
            Section **ps = &si->sec;
            while (*ps)
                ps = &(*ps)->prev;
            *ps = s, s->prev = NULL;
            si->data_size = si->sh_size;
        }

        if (s->sh_flags & SHF_TLS) {
            strcpy(si->name, ".tls");
            pe_build_tls(pe, s);
        }

        //printf("%08x %05x %08x %s\n", si->sh_addr, si->sh_size, si->pe_flags, s->name);
    }
#if 0
    for (i = 1; i < nbs; ++i) {
        Section *s = s1->sections[sec_order[i]];
        int type = s->sh_type;
        int flags = s->sh_flags;
        printf("section %-16s %-10s %p %04x %s,%s,%s\n",
            s->name,
            type == SHT_PROGBITS ? "progbits" :
            type == SHT_INIT_ARRAY ? "initarr" :
            type == SHT_FINI_ARRAY ? "finiarr" :
            type == SHT_NOBITS ? "nobits" :
            type == SHT_SYMTAB ? "symtab" :
            type == SHT_STRTAB ? "strtab" :
            type == SHT_RELX ? "rel" : "???",
            s->sh_addr,
            (unsigned)s->data_offset,
            flags & SHF_ALLOC ? "alloc" : "",
            flags & SHF_WRITE ? "write" : "",
            flags & SHF_EXECINSTR ? "exec" : ""
            );
        fflush(stdout);
    }
    s1->verbose = 2;
#endif
    tcc_free(sec_order);
    return 0;
}

/*----------------------------------------------------------------------------*/
static int pe_check_symbols(struct pe_info *pe)
{
    int sym_index, sym_end;
    int ret = 0;
    TCCState *s1 = pe->s1;

    pe_align_section(text_section, 8);

    sym_end = symtab_section->data_offset / sizeof(ElfW(Sym));
    for (sym_index = 1; sym_index < sym_end; ++sym_index) {
        ElfW(Sym) *sym = (ElfW(Sym) *)symtab_section->data + sym_index;
        if (sym->st_shndx == SHN_UNDEF) {
            const char *name = (char*)symtab_section->link->data + sym->st_name;
            unsigned type = ELFW(ST_TYPE)(sym->st_info);
            int imp_sym = 0;
            struct import_symbol *is;

            int _imp_, n;
            char buffer[200];
            const char *s, *p;

            n = _imp_ = 0;
            if (sym->st_other & ST_PE_IMPORT)
                _imp_ = 1;
            do {
                s = pe_export_name(s1, sym);
                if (n) {
                    /* second try: */
                    if (sym->st_other & ST_PE_STDCALL) {
                        /* try w/0 stdcall deco (windows API convention) */
                        p = strrchr(s, '@');
                        if (!p || s[0] != '_')
                            break;
                        strcpy(buffer, s+1)[p-s-1] = 0, s = buffer;
                    } else if (s[0] != '_') { /* try non-ansi function */
                        buffer[0] = '_', strcpy(buffer + 1, s), s = buffer;
                    } else if (0 == memcmp(s, "_imp__", 6)) { /* mingw 3.7 */
                        s += 6, _imp_ = 1;
                    } else if (0 == memcmp(s, "__imp_", 6)) { /* mingw 2.0 */
                        s += 6, _imp_ = 1;
                    } else {
                        break;
                    }
                }
                imp_sym = find_elf_sym(s1->dynsymtab_section, s);
            } while (0 == imp_sym && ++n < 2);

            //printf("pe_find_export (%d) %4x %s\n", n, imp_sym, name);
            if (0 == imp_sym)
                continue; /* will throw the 'undefined' error in relocate_syms() */

            if (s1->pe_all_delay
                && (type == STT_FUNC || (type == STT_NOTYPE && 0 == _imp_)))
                is = pe_add_delay_import(pe, imp_sym);
            else
                is = pe_add_import(pe, imp_sym);

            if (type == STT_FUNC
                /* symbols from assembler often have no type */
                || (type == STT_NOTYPE && 0 == _imp_)) {
                unsigned offset = is->thk_offset;
                if (offset) {
                    /* got aliased symbol, like stricmp and _stricmp */
                } else if (s1->pe_all_delay) {
                    /* Reserve room for a delay-load stub (a per-function
                       thunk that loads the address of its delay-import
                       address-table slot and jumps to a shared per-DLL
                       tail-merge stub).  The actual code and relocations
                       are filled in later by pe_build_delay_imports(),
                       once the tail-merge stub and the delay import
                       tables have been laid out. */
                    offset = text_section->data_offset;
                    is->thk_offset = offset;
#if defined(TCC_TARGET_X86_64)
                    section_ptr_add(text_section, 7 + 5);
#elif defined(TCC_TARGET_I386)
                    section_ptr_add(text_section, 5 + 5);
#else
                    tcc_error_noabort(
                        "--delay-all is only supported for the i386 and x86_64 targets");
#endif
                } else {
                    unsigned char *p;

                    /* add a helper symbol, will be patched later in
                       pe_build_imports */
                    sprintf(buffer, "IAT.%s", name);
                    is->iat_index = put_elf_sym(
                        symtab_section, 0, sizeof(DWORD),
                        ELFW(ST_INFO)(STB_LOCAL, STT_OBJECT),
                        0, SHN_UNDEF, buffer);

                    offset = text_section->data_offset;
                    is->thk_offset = offset;

                    /* add the 'jmp IAT[x]' instruction */
#ifdef TCC_TARGET_ARM
                    p = section_ptr_add(text_section, 8+4); // room for code and address
                    write32le(p + 0, 0xE59FC000); // arm code ldr ip, [pc] ; PC+8+0 = 0001xxxx
                    write32le(p + 4, 0xE59CF000); // arm code ldr pc, [ip]
                    put_elf_reloc(symtab_section, text_section,
                        offset + 8, R_XXX_THUNKFIX, is->iat_index); // offset to IAT position
#elif defined(TCC_TARGET_ARM64)
                    p = section_ptr_add(text_section, 24);
                    write32le(p + 0, 0x58000090); /* ldr x16, [pc, #16] */
                    write32le(p + 4, 0xf9400210); /* ldr x16, [x16] */
                    write32le(p + 8, 0xd61f0200); /* br x16 */
                    write32le(p + 12, 0xd503201f); /* nop for alignment */
                    put_elf_reloc(symtab_section, text_section,
                        offset + 16, R_XXX_THUNKFIX, is->iat_index);
#else
                    p = section_ptr_add(text_section, 8);
                    write16le(p, 0x25FF);
#ifdef TCC_TARGET_X86_64
                    write32le(p + 2, (DWORD)-4);
#endif
                    put_elf_reloc(symtab_section, text_section, 
                        offset + 2, R_XXX_THUNKFIX, is->iat_index);
#endif
                }
                /* tcc_realloc might have altered sym's address */
                sym = (ElfW(Sym) *)symtab_section->data + sym_index;
                /* patch the original symbol */
                sym->st_value = offset;
                sym->st_shndx = text_section->sh_num;
                sym->st_other &= ~ST_PE_EXPORT; /* do not export */

            } else { /* STT_OBJECT */
                if (0 == _imp_)
                    ret = tcc_error_noabort("symbol '%s' is missing __declspec(dllimport)", name);
                /* data symbols can't be delay-loaded transparently (the
                   address would need re-fetching on every access), so
                   they stay ordinary imports even under --delay-all;
                   original symbol will be patched later in pe_build_imports */
                sym->st_value = is->iat_index; /* chain potential alias */
                is->iat_index = sym_index;
            }

        } else if (s1->rdynamic
                   && ELFW(ST_BIND)(sym->st_info) != STB_LOCAL) {
            /* if -rdynamic option, then export all non local symbols */
            sym->st_other |= ST_PE_EXPORT;
        }
    }
    return ret;
}

/*----------------------------------------------------------------------------*/
#if PE_PRINT_SECTIONS
static void pe_print_section(FILE * f, Section * s)
{
    /* just if you're curious */
    BYTE *p, *e, b;
    int i, n, l, m;
    p = s->data;
    e = s->data + s->data_offset;
    l = e - p;

    fprintf(f, "section  \"%s\"", s->name);
    if (s->link)
        fprintf(f, "\nlink     \"%s\"", s->link->name);
    if (s->reloc)
        fprintf(f, "\nreloc    \"%s\"", s->reloc->name);
    fprintf(f, "\nv_addr   %08X", (unsigned)s->sh_addr);
    fprintf(f, "\ncontents %08X", (unsigned)l);
    fprintf(f, "\n\n");

    if (s->sh_type == SHT_NOBITS)
        return;

    if (0 == l)
        return;

    if (s->sh_type == SHT_SYMTAB)
        m = sizeof(ElfW(Sym));
    else if (s->sh_type == SHT_RELX)
        m = sizeof(ElfW_Rel);
    else
        m = 16;

    fprintf(f, "%-8s", "offset");
    for (i = 0; i < m; ++i)
        fprintf(f, " %02x", i);
    n = 56;

    if (s->sh_type == SHT_SYMTAB || s->sh_type == SHT_RELX) {
        const char *fields1[] = {
            "name",
            "value",
            "size",
            "bind",
            "type",
            "other",
            "shndx",
            NULL
        };

        const char *fields2[] = {
            "offs",
            "type",
            "symb",
            NULL
        };

        const char **p;

        if (s->sh_type == SHT_SYMTAB)
            p = fields1, n = 106;
        else
            p = fields2, n = 58;

        for (i = 0; p[i]; ++i)
            fprintf(f, "%6s", p[i]);
        fprintf(f, "  symbol");
    }

    fprintf(f, "\n");
    for (i = 0; i < n; ++i)
        fprintf(f, "-");
    fprintf(f, "\n");

    for (i = 0; i < l;)
    {
        fprintf(f, "%08X", i);
        for (n = 0; n < m; ++n) {
            if (n + i < l)
                fprintf(f, " %02X", p[i + n]);
            else
                fprintf(f, "   ");
        }

        if (s->sh_type == SHT_SYMTAB) {
            ElfW(Sym) *sym = (ElfW(Sym) *) (p + i);
            const char *name = s->link->data + sym->st_name;
            fprintf(f, "  %04X  %04X  %04X   %02X    %02X    %02X   %04X  \"%s\"",
                    (unsigned)sym->st_name,
                    (unsigned)sym->st_value,
                    (unsigned)sym->st_size,
                    (unsigned)ELFW(ST_BIND)(sym->st_info),
                    (unsigned)ELFW(ST_TYPE)(sym->st_info),
                    (unsigned)sym->st_other,
                    (unsigned)sym->st_shndx,
                    name);

        } else if (s->sh_type == SHT_RELX) {
            ElfW_Rel *rel = (ElfW_Rel *) (p + i);
            ElfW(Sym) *sym =
                (ElfW(Sym) *) s->link->data + ELFW(R_SYM)(rel->r_info);
            const char *name = s->link->link->data + sym->st_name;
            fprintf(f, "  %04X   %02X   %04X  \"%s\"",
                    (unsigned)rel->r_offset,
                    (unsigned)ELFW(R_TYPE)(rel->r_info),
                    (unsigned)ELFW(R_SYM)(rel->r_info),
                    name);
        } else {
            fprintf(f, "   ");
            for (n = 0; n < m; ++n) {
                if (n + i < l) {
                    b = p[i + n];
                    if (b < 32 || b >= 127)
                        b = '.';
                    fprintf(f, "%c", b);
                }
            }
        }
        i += m;
        fprintf(f, "\n");
    }
    fprintf(f, "\n\n");
}

static void pe_print_sections(TCCState *s1, const char *fname)
{
    Section *s;
    FILE *f;
    int i;
    f = fopen(fname, "w");
    for (i = 1; i < s1->nb_sections; ++i) {
        s = s1->sections[i];
        pe_print_section(f, s);
    }
    pe_print_section(f, s1->dynsymtab_section);
    fclose(f);
}
#endif

/* ------------------------------------------------------------- */

ST_FUNC int pe_putimport(TCCState *s1, int dllindex, const char *name, addr_t value)
{
    return set_elf_sym(
        s1->dynsymtab_section,
        value,
        dllindex, /* st_size */
        ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE),
        0,
        value ? SHN_ABS : SHN_UNDEF,
        name
        );
}

static int read_mem(int fd, unsigned offset, void *buffer, unsigned len)
{
    lseek(fd, offset, SEEK_SET);
    return len == read(fd, buffer, len);
}

/* ------------------------------------------------------------- */

static int get_dllexports(int fd, char **pp)
{
    int i, k, l, n, n0, ret;
    char *p;

    IMAGE_SECTION_HEADER ish;
    IMAGE_EXPORT_DIRECTORY ied;
    IMAGE_DOS_HEADER dh;
    IMAGE_FILE_HEADER ih;
    DWORD sig, ref, addr;
    DWORD *namep = NULL, p0 = 0, p1;

    int pef_hdroffset, opt_hdroffset, sec_hdroffset;

    n = n0 = 0;
    p = NULL;
    ret = 1;
    if (!read_mem(fd, 0, &dh, sizeof dh))
        goto the_end;
    if (!read_mem(fd, dh.e_lfanew, &sig, sizeof sig))
        goto the_end;
    if (sig != 0x00004550)
        goto the_end;
    pef_hdroffset = dh.e_lfanew + sizeof sig;
    if (!read_mem(fd, pef_hdroffset, &ih, sizeof ih))
        goto the_end;
    opt_hdroffset = pef_hdroffset + sizeof ih;
    if (ih.Machine == 0x014C || ih.Machine == 0x01C0) {
        IMAGE_OPTIONAL_HEADER32 oh;
        sec_hdroffset = opt_hdroffset + sizeof oh;
        if (!read_mem(fd, opt_hdroffset, &oh, sizeof oh))
            goto the_end;
        if (IMAGE_DIRECTORY_ENTRY_EXPORT >= oh.NumberOfRvaAndSizes)
            goto the_end_0;
        addr = oh.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    } else if (ih.Machine == 0x8664 || ih.Machine == 0xAA64) {
        IMAGE_OPTIONAL_HEADER64 oh;
        sec_hdroffset = opt_hdroffset + sizeof oh;
        if (!read_mem(fd, opt_hdroffset, &oh, sizeof oh))
            goto the_end;
        if (IMAGE_DIRECTORY_ENTRY_EXPORT >= oh.NumberOfRvaAndSizes)
            goto the_end_0;
        addr = oh.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    } else
        goto the_end;

    //printf("addr: %08x\n", addr);
    for (i = 0; i < ih.NumberOfSections; ++i) {
        if (!read_mem(fd, sec_hdroffset + i * sizeof ish, &ish, sizeof ish))
            goto the_end;
        //printf("vaddr: %08x\n", ish.VirtualAddress);
        if (addr >= ish.VirtualAddress && addr < ish.VirtualAddress + ish.SizeOfRawData)
            goto found;
    }
    goto the_end_0;
found:
    ref = ish.VirtualAddress - ish.PointerToRawData;
    if (!read_mem(fd, addr - ref, &ied, sizeof ied))
        goto the_end;
    k = ied.NumberOfNames;
    if (k) {
        namep = tcc_malloc(l = k * sizeof *namep);
        if (!read_mem(fd, ied.AddressOfNames - ref, namep, l))
            goto the_end;
        for (i = l = 0; i < k; ++i) {
            p1 = namep[i] - ref;
            if (p1 != p0)
                lseek(fd, p0 = p1, SEEK_SET), l = 0;
            do {
                if (0 == l) {
                    if (n + 1000 >= n0)
                        p = tcc_realloc(p, n0 += 1000);
                    if ((l = read(fd, p + n, 1000 - 1)) <= 0)
                        goto the_end;
                }
                --l, ++p0;
            } while (p[n++]);
        }
        p[n] = 0;
    }
the_end_0:
    ret = 0;
the_end:
    tcc_free(namep);
    if (ret && p)
        tcc_free(p), p = NULL;
    *pp = p;
    return ret;
}

/* -------------------------------------------------------------
 *  This is for compiled windows resources in 'coff' format
 *  as generated by 'windres.exe -O coff ...'.
 */

static int pe_load_res(TCCState *s1, int fd)
{
    struct pe_rsrc_header hdr;
    Section *rsrc_section;
    int i, ret = -1, sym_index;
    BYTE *ptr;
    unsigned offs;

    if (!read_mem(fd, 0, &hdr, sizeof hdr))
        goto quit;

    if (hdr.filehdr.Machine != IMAGE_FILE_MACHINE
        || hdr.filehdr.NumberOfSections != 1
        || strcmp((char*)hdr.sectionhdr.Name, ".rsrc") != 0)
        goto quit;

    rsrc_section = new_section(s1, ".rsrc", SHT_PROGBITS, SHF_ALLOC);
    ptr = section_ptr_add(rsrc_section, hdr.sectionhdr.SizeOfRawData);
    offs = hdr.sectionhdr.PointerToRawData;
    if (!read_mem(fd, offs, ptr, hdr.sectionhdr.SizeOfRawData))
        goto quit;
    offs = hdr.sectionhdr.PointerToRelocations;
    sym_index = put_elf_sym(symtab_section, 0, 0, 0, 0, rsrc_section->sh_num, ".rsrc");
    for (i = 0; i < hdr.sectionhdr.NumberOfRelocations; ++i) {
        struct pe_rsrc_reloc rel;
        if (!read_mem(fd, offs, &rel, sizeof rel))
            goto quit;
        // printf("rsrc_reloc: %x %x %x\n", rel.offset, rel.size, rel.type);
        if (rel.type != RSRC_RELTYPE)
            goto quit;
        put_elf_reloc(symtab_section, rsrc_section,
            rel.offset, R_XXX_RELATIVE, sym_index);
        offs += sizeof rel;
    }
    ret = 0;
quit:
    return ret;
}

/* ------------------------------------------------------------- */

static char *trimfront(char *p)
{
    while ((unsigned char)*p <= ' ' && *p && *p != '\n')
	++p;
    return p;
}

/*
static char *trimback(char *a, char *e)
{
    while (e > a && (unsigned char)e[-1] <= ' ')
	--e;
    *e = 0;;
    return a;
}*/

static char *get_token(char **s, char *f)
{
    char *p, *e;
    int q;

    p = trimfront(*s);
    q = *p;
    if (q == '"') /* support quoted LIBRARY "xyz.dll" */
        ++p;
    else
        q = ' ';
    for (e = p; (unsigned char)*e >= ' ' && *e != q; ++e)
        ;
    if (*e == '"')
        *e++ = 0;
    *s = trimfront(e);
    *f = **s, *e = 0;
    return p;
}

static int pe_load_def(TCCState *s1, int fd)
{
    int state = 0, ret = -1, dllindex = 0, ord;
    char dllname[80], *buf, *line, *p, *x, next;

    buf = tcc_load_text(fd);
    if (!buf)
        return ret;

    for (line = buf;; ++line)  {
        p = get_token(&line, &next);
        if (!(*p && *p != ';'))
            goto skip;
        switch (state) {
        case 0:
            if (0 != stricmp(p, "LIBRARY") || next == '\n')
                goto quit;
            pstrcpy(dllname, sizeof dllname, get_token(&line, &next));
            if (!*tcc_fileextension(dllname))
                pstrcat(dllname, sizeof dllname, ".dll");
            ++state;
            break;
        case 1:
            if (0 != stricmp(p, "EXPORTS"))
                goto quit;
            ++state;
            break;
        case 2:
            dllindex = tcc_add_dllref(s1, dllname, 0)->index;
            ++state;
            /* fall through */
        default:
            /* get ordinal and will store in sym->st_value */
            ord = 0;
            if (next == '@') {
                x = get_token(&line, &next);
                ord = (int)strtol(x + 1, &x, 10);
            }
            //printf("token %s ; %s : %d\n", dllname, p, ord);
            pe_putimport(s1, dllindex, p, ord);
            break;
        }
skip:
        while ((unsigned char)next > ' ')
            get_token(&line, &next);
        if (next != '\n')
            break;
    }
    ret = 0;
quit:
    tcc_free(buf);
    return ret;
}

/* ------------------------------------------------------------- */

static int pe_load_dll(TCCState *s1, int fd, const char *filename)
{
    char *p, *q;
    DLLReference *ref = tcc_add_dllref(s1, filename, 0);
    if (ref->found)
        return 0;
    if (get_dllexports(fd, &p))
        return -1;
    if (p) {
        for (q = p; *q; q += 1 + strlen(q))
            pe_putimport(s1, ref->index, q, 0);
        tcc_free(p);
    }
    return 0;
}

ST_FUNC int pe_load_file(struct TCCState *s1, int fd, const char *filename)
{
    int ret = -1;
    char buf[10];
    if (0 == strcmp(tcc_fileextension(filename), ".def"))
        ret = pe_load_def(s1, fd);
    else if (pe_load_res(s1, fd) == 0)
        ret = 0;
    else if (read_mem(fd, 0, buf, 4) && 0 == memcmp(buf, "MZ", 2))
        ret = pe_load_dll(s1, fd, filename);
    return ret;
}

/* ------------------------------------------------------------- */
/* PE-COFF relocatable object file reader.
 *
 * This lets the win32 targets consume genuine ".o" files produced by GNU as /
 * MinGW binutils, by translating them into tcc's internal, ELF-shaped
 * representation.  It is input-only: tcc still *writes* ELF objects for -c
 * (libtcc.c, "always elf for objects").
 *
 * References, cited per item below:
 *   [PECOFF] Microsoft PE and COFF Specification, sections 3 (COFF File
 *            Header), 4 / 4.1 (Section Table, Section Flags), 5.2.1
 *            (Relocations - Type Indicators), 5.3 (COFF Relocations),
 *            5.4 / 5.4.4 / 5.4.5 (COFF Symbol Table, Section Number Values,
 *            Storage Class), 5.5.5 (Auxiliary Format 5: Section
 *            Definitions), 5.6 (COFF String Table).
 *   [BFD]    binutils sources: include/coff/internal.h, include/coff/i386.h,
 *            include/coff/x86_64.h, include/coff/pe.h,
 *            include/coff/external.h, bfd/coff-i386.c, bfd/coff-x86_64.c.
 *
 * ADDEND MODEL - read this before touching the relocation code.
 *
 * COFF relocations are REL-style: the 10-byte relocation record has no addend
 * field, the addend lives in the section contents at the fixup site
 * ([PECOFF] 5.3).  That is the same shape as ELF REL, so on i386, where tcc
 * is a REL target, the stored value is reused as-is.
 *
 * What is *not* the same is PC-relative relocations.  PE measures the
 * displacement from the byte *following* the field, whereas ELF R_386_PC32 /
 * R_X86_64_PC32 compute S + A - P with P at the *start* of the field.  So the
 * ELF addend is the stored value minus the field size (minus 4+N for the
 * x86-64 REL32_N variants, minus 8 for the 64-bit PCRQUAD).  This is exactly
 * the "*addendp -= 4" that binutils applies under COFF_WITH_PE in
 * bfd/coff-i386.c:coff_i386_rtype_to_howto and
 * bfd/coff-x86_64.c:coff_amd64_rtype_to_howto.  It was also verified
 * empirically: assembling "call _extfunc" with i686-w64-mingw32-as stores
 * 00000000 at the fixup where the elf32-i386 assembler stores fffffffc.
 *
 * On i386 (REL) the bias is folded into the section contents in place.  On
 * x86-64 (RELA) the whole addend is lifted out of the section contents into
 * r_addend and the field is zeroed - necessary because tcc's relocate() *adds*
 * to the field (add32le/add64le), which would otherwise apply it twice.
 *
 * Relocation types that are not translated explicitly below are a hard error.
 * They are never passed through unchanged and never skipped: a numerically
 * preserved but semantically wrong relocation type produces corrupt output
 * indistinguishable from correct output, which is the exact failure this
 * reader exists to make impossible.
 */

/* COFF file header, [PECOFF] 3.3 / [BFD] include/coff/external.h FILHSZ */
#define COFF_FILHSZ         20
#define FH_MACHINE           0  /* WORD  */
#define FH_NSCNS             2  /* WORD  */
#define FH_SYMPTR            8  /* DWORD */
#define FH_NSYMS            12  /* DWORD */
#define FH_OPTHDR           16  /* WORD  */

/* section table entry, [PECOFF] 4 / [BFD] include/coff/external.h SCNHSZ */
#define COFF_SCNHSZ         40
#define SH_NAME              0  /* 8 bytes */
#define SH_VADDR            12  /* DWORD */
#define SH_RAWSIZE          16  /* DWORD */
#define SH_RAWPTR           20  /* DWORD */
#define SH_RELPTR           24  /* DWORD */
#define SH_NRELOC           32  /* WORD  */
#define SH_FLAGS            36  /* DWORD */

/* symbol table entry, [PECOFF] 5.4 / [BFD] include/coff/external.h SYMESZ */
#define COFF_SYMESZ         18
#define SY_NAME              0  /* 8 bytes */
#define SY_VALUE             8  /* DWORD */
#define SY_SCNUM            12  /* signed WORD */
#define SY_SCLASS           16  /* BYTE */
#define SY_NUMAUX           17  /* BYTE */

/* auxiliary section definition record, [PECOFF] 5.5.5 / [BFD]
   include/coff/external.h "union external_auxent", member x_scn */
#define AUX_SCN_ASSOC       12  /* WORD, 1-based section number */
#define AUX_SCN_SELECTION   14  /* BYTE */

/* relocation record, [PECOFF] 5.3 / [BFD] include/coff/external.h RELSZ */
#define COFF_RELSZ          10
#define RE_VADDR             0  /* DWORD */
#define RE_SYMNDX            4  /* DWORD */
#define RE_TYPE              8  /* WORD  */

/* section numbers, [PECOFF] 5.4.4 / [BFD] include/coff/internal.h */
#define COFF_N_UNDEF         0
#define COFF_N_ABS         (-1)

/* storage classes, [PECOFF] 5.4.5 / [BFD] include/coff/internal.h */
#define COFF_C_EXT           2
#define COFF_C_STAT          3
#define COFF_C_LABEL         6
#define COFF_C_NT_WEAK     105
#define COFF_C_WEAKEXT     127

/* extra section flags used here, [PECOFF] 4.1 / [BFD] include/coff/pe.h */
#define IMAGE_SCN_LNK_INFO          0x00000200
#define IMAGE_SCN_LNK_REMOVE        0x00000800
#define IMAGE_SCN_LNK_COMDAT        0x00001000
#define IMAGE_SCN_LNK_NRELOC_OVFL   0x01000000
#define IMAGE_SCN_ALIGN_MASK        0x00F00000
#define IMAGE_SCN_ALIGN_SHIFT               20

/* COMDAT selection values, [PECOFF] 5.5.5 / [BFD] include/coff/pe.h */
#define COFF_COMDAT_EXACT_MATCH      4
#define COFF_COMDAT_ASSOCIATIVE      5

#if defined TCC_TARGET_I386
# define R_XXX_NONE R_386_NONE
#elif defined TCC_TARGET_X86_64
# define R_XXX_NONE R_X86_64_NONE
#else
# define R_XXX_NONE 0
#endif

typedef struct CoffSection {
    Section *s;             /* tcc section this one was merged into, or NULL */
    unsigned long offset;   /* where it landed inside s */
    unsigned char *hdr;     /* the 40-byte COFF section header */
    int sym;                /* index of its section symbol, -1 if none */
    unsigned char comdat;   /* COMDAT selection value, 0 if not a COMDAT */
    unsigned char keep;     /* selected for merging */
    unsigned char dup;      /* COMDAT duplicate: dropped, symbols redirected */
} CoffSection;

/* Resolve a COFF name field: 8 bytes holding either an inline (possibly
   unterminated) name or, when the first four bytes are zero, an offset into
   the string table.  [PECOFF] 5.4.1 and 5.6.
   Section headers cannot use that encoding - their whole 8 bytes are the
   name - so a long section name is instead written as '/' followed by the
   decimal string-table offset ([PECOFF] 4, "Name" field); pass IS_SEC to
   accept that form. */
static const char *coff_name(unsigned char *field, char *buf,
                             char *strtab, unsigned long strtab_size, int is_sec)
{
    unsigned long off;
    if (read32le(field) == 0) {
        off = read32le(field + 4);
        if (!strtab || off < 4 || off >= strtab_size)
            return NULL;
        return strtab + off;
    }
    memcpy(buf, field, 8);
    buf[8] = 0;
    if (is_sec && buf[0] == '/') {
        char *e;
        /* binutils also emits a base64 form, "//", for offsets that do not
           fit in seven digits; it is not decoded here and must not be
           mistaken for a decimal offset. */
        if (buf[1] < '0' || buf[1] > '9')
            return NULL;
        off = strtoul(buf + 1, &e, 10);
        if (*e || off < 4 || off >= strtab_size)
            return NULL;
        return strtab + off;
    }
    return buf;
}

/* Translate one COFF relocation type into tcc's internal (ELF) type.
   Returns the ELF type, or -1 if the type is not handled - the caller then
   errors out, loudly, naming the numeric type.
   *pfsize gets the size in bytes of the field being patched (0 for no-ops).
   PTR is the fixup site, or NULL when the section has no contents.
   On a RELA target *paddend receives the addend lifted out of the contents;
   on a REL target *pbias receives the amount to add to the stored value in
   place.  Exactly one of the two mechanisms is ever used, per target. */
static int coff_reloc_type(int coff_type, unsigned char *ptr,
                           addr_t *paddend, int *pbias, int *pfsize)
{
    *paddend = 0;
    *pbias = 0;
    *pfsize = 0;
#if defined TCC_TARGET_I386
    /* i386: tcc is a REL target - the addend stays in the section contents,
       so only the PC-relative bias has to be folded in.
       Type numbers: [PECOFF] 5.2.1 "x86 Processors", and [BFD]
       include/coff/i386.h; semantics from [BFD] bfd/coff-i386.c howto_table
       and coff_i386_rtype_to_howto. */
    switch (coff_type) {
    case 0x0000:
        /* IMAGE_REL_I386_ABSOLUTE: "The relocation is ignored" ([PECOFF]
           5.2.1).  R_386_NONE is tcc's no-op.  No field, no addend. */
        return R_386_NONE;
    case 0x0006:
        /* IMAGE_REL_I386_DIR32 (= R_DIR32, [BFD] coff/i386.h): the 32-bit
           virtual address of the target.  Not PC-relative; 4-byte field;
           addend stored in the field.  Identical to ELF R_386_32 (S + A), so
           no bias.  Verified empirically: gas stores the same bytes for
           pe-i386 dir32 and elf32-i386 R_386_32. */
        *pfsize = 4;
        return R_386_32;
    case 0x0014:
        /* IMAGE_REL_I386_REL32 (= R_PCRLONG, objdump "DISP32"): 32-bit
           PC-relative displacement measured from the byte following the
           4-byte field.  ELF R_386_PC32 is S + A - P with P at the start of
           the field, so the ELF addend is 4 less than what COFF stores.
           [BFD] bfd/coff-i386.c coff_i386_rtype_to_howto: "*addendp -= 4"
           under COFF_WITH_PE. */
        *pfsize = 4;
        *pbias = -4;
        return R_386_PC32;
    }
    /* Deliberately unhandled, and therefore hard errors:
         0x0007 IMAGE_REL_I386_DIR32NB - RVA relative to the image base; tcc
                has no internal relocation with that meaning.
         0x000A SECTION, 0x000B SECREL, 0x000C TOKEN, 0x000D SECREL7 -
                section index / section-relative; no tcc equivalent.
         0x0001 DIR16, 0x0002 REL16, and the non-PE legacy COFF types 15..19
                (R_RELBYTE, R_RELWORD, R_RELLONG, R_PCRBYTE, R_PCRWORD from
                [BFD] coff/i386.h) - not emitted by gas for pe-i386, and
                mapping them from memory is exactly the guess this reader
                refuses to make. */
#elif defined TCC_TARGET_X86_64
    /* x86-64: tcc is a RELA target, so the addend is lifted out of the
       section contents into r_addend and the field is zeroed by the caller.
       Type numbers: [PECOFF] 5.2.1 "x64 Processors", and [BFD]
       include/coff/x86_64.h; biases from [BFD] bfd/coff-x86_64.c
       coff_amd64_rtype_to_howto. */
    if (coff_type != 0 && !ptr)
        return -1;      /* needs to read an addend, but there are no contents */
    switch (coff_type) {
    case 0:
        /* IMAGE_REL_AMD64_ABSOLUTE: ignored ([PECOFF] 5.2.1). */
        return R_X86_64_NONE;
    case 1:
        /* IMAGE_REL_AMD64_ADDR64 (= R_AMD64_DIR64): 64-bit VA, not
           PC-relative, 8-byte field.  ELF R_X86_64_64 (S + A). */
        *pfsize = 8;
        *paddend = (addr_t)read64le(ptr);
        return R_X86_64_64;
    case 2:
        /* IMAGE_REL_AMD64_ADDR32 (= R_AMD64_DIR32): 32-bit VA, 4-byte field.
           The [BFD] howto complains on *bitfield* overflow (unsigned), which
           matches ELF R_X86_64_32, not the sign-extending R_X86_64_32S. */
        *pfsize = 4;
        *paddend = (addr_t)read32le(ptr);
        return R_X86_64_32;
    case 4: /* IMAGE_REL_AMD64_REL32   (= R_AMD64_PCRLONG)   */
    case 5: /* IMAGE_REL_AMD64_REL32_1 (= R_AMD64_PCRLONG_1) */
    case 6: /* IMAGE_REL_AMD64_REL32_2 */
    case 7: /* IMAGE_REL_AMD64_REL32_3 */
    case 8: /* IMAGE_REL_AMD64_REL32_4 */
    case 9: /* IMAGE_REL_AMD64_REL32_5 */
        /* 32-bit PC-relative displacement from the byte following the 4-byte
           field (REL32), or from N bytes beyond that (REL32_N, N = type - 4).
           ELF R_X86_64_PC32 measures from the start of the field, hence
           -4-N.  [BFD] bfd/coff-x86_64.c coff_amd64_rtype_to_howto:
           "*addendp -= (bfd_vma)(rel->r_type - R_AMD64_PCRLONG)" followed by
           "*addendp -= 4". */
        *pfsize = 4;
        *paddend = (addr_t)(int64_t)((int)read32le(ptr) - 4 - (coff_type - 4));
        return R_X86_64_PC32;
    case 14:
        /* R_AMD64_PCRQUAD: a gas extension, named "R_X86_64_PC64" in the
           [BFD] bfd/coff-x86_64.c howto table - 64-bit PC-relative, 8-byte
           field, bias -8 ("if (rel->r_type == R_AMD64_PCRQUAD) *addendp -=
           8" in coff_amd64_rtype_to_howto). */
        *pfsize = 8;
        *paddend = (addr_t)(read64le(ptr) - 8);
        return R_X86_64_PC64;
    }
    /* Deliberately unhandled, and therefore hard errors:
         3  IMAGE_REL_AMD64_ADDR32NB - RVA; no tcc equivalent.
         10 SECTION, 11 SECREL, 12 SECREL7, 13 TOKEN - section index /
            section-relative; no tcc equivalent.
         15..20 - non-PE legacy COFF types from [BFD] coff/x86_64.h. */
#else
    /* Other PE targets (arm-wince, arm64-win32) use different relocation
       numbering, which this reader does not implement; every type errors. */
    (void)ptr;
#endif
    return -1;
}

ST_FUNC int pe_load_obj_file(TCCState *s1, int fd, unsigned long file_offset)
{
    unsigned char fh[COFF_FILHSZ];
    unsigned char *shdrs = NULL, *symtab = NULL, *relocs = NULL;
    char *strtab = NULL;
    unsigned long strtab_size = 0, symptr, size;
    int nsec = 0, nsyms = 0, i, j, ret = -1;
    CoffSection *sec = NULL;
    int *old_to_new = NULL;
    char nbuf[9], nbuf2[9];

    if (!read_mem(fd, file_offset, fh, COFF_FILHSZ))
        return tcc_error_noabort("invalid PE-COFF object file (truncated header)");
    if (read16le(fh + FH_MACHINE) != IMAGE_FILE_MACHINE)
        return tcc_error_noabort("PE-COFF object file for machine 0x%04x, expected 0x%04x",
            (unsigned)read16le(fh + FH_MACHINE), (unsigned)IMAGE_FILE_MACHINE);

    nsec = read16le(fh + FH_NSCNS);
    nsyms = read32le(fh + FH_NSYMS);
    symptr = read32le(fh + FH_SYMPTR);
    if (nsec <= 0)
        return tcc_error_noabort("PE-COFF object file has no sections");
    if (nsyms < 0)
        return tcc_error_noabort("PE-COFF object file has a bad symbol count");

    shdrs = load_data(fd, file_offset + COFF_FILHSZ + read16le(fh + FH_OPTHDR),
                      (unsigned long)nsec * COFF_SCNHSZ);
    sec = tcc_mallocz(sizeof(CoffSection) * (nsec + 1));
    for (i = 1; i <= nsec; i++) {
        sec[i].hdr = shdrs + (i - 1) * COFF_SCNHSZ;
        sec[i].sym = -1;
    }

    if (!symptr)
        nsyms = 0;      /* no symbol table: nothing to resolve against */
    if (nsyms) {
        unsigned char sz[4];
        symtab = load_data(fd, file_offset + symptr,
                           (unsigned long)nsyms * COFF_SYMESZ);
        /* the string table follows the symbol table and begins with its own
           total size in bytes ([PECOFF] 5.6) */
        if (read_mem(fd, file_offset + symptr + nsyms * COFF_SYMESZ, sz, 4)
            && read32le(sz) >= 4) {
            strtab_size = read32le(sz);
            strtab = load_data(fd, file_offset + symptr + nsyms * COFF_SYMESZ,
                               strtab_size);
            strtab[strtab_size - 1] = 0;  /* a corrupt table cannot run off */
        }
    }
    old_to_new = tcc_mallocz((nsyms + 1) * sizeof(int));

    /* --- pass 1: locate section symbols and COMDAT selections ---------- */
    for (i = 0; i < nsyms; ) {
        unsigned char *sy = symtab + i * COFF_SYMESZ;
        int numaux = sy[SY_NUMAUX];
        int scnum = (int16_t)read16le(sy + SY_SCNUM);
        if (sy[SY_SCLASS] == COFF_C_STAT && numaux >= 1 && i + numaux < nsyms
            && scnum > 0 && scnum <= nsec && read32le(sy + SY_VALUE) == 0
            && sec[scnum].sym < 0) {
            const char *n1 = coff_name(sy + SY_NAME, nbuf, strtab, strtab_size, 0);
            const char *n2 = coff_name(sec[scnum].hdr + SH_NAME, nbuf2, strtab, strtab_size, 1);
            /* a section definition symbol carries the section's own name
               ([PECOFF] 5.5.5); an ordinary C_STAT local does not */
            if (n1 && n2 && 0 == strcmp(n1, n2)) {
                sec[scnum].sym = i;
                if (read32le(sec[scnum].hdr + SH_FLAGS) & IMAGE_SCN_LNK_COMDAT)
                    sec[scnum].comdat = sy[COFF_SYMESZ + AUX_SCN_SELECTION];
            }
        }
        i += 1 + numaux;
    }

    /* --- pass 2: decide which sections to keep ------------------------- */
    for (i = 1; i <= nsec; i++) {
        unsigned flags = read32le(sec[i].hdr + SH_FLAGS);
        const char *name = coff_name(sec[i].hdr + SH_NAME, nbuf, strtab, strtab_size, 1);
        int sel = sec[i].comdat;

        if (!name) {
            tcc_error_noabort("PE-COFF object: bad name for section %d", i);
            goto the_end;
        }
        /* linker directives (.drectve) and other non-contributing sections */
        if (flags & (IMAGE_SCN_LNK_REMOVE | IMAGE_SCN_LNK_INFO))
            continue;
        /* debug information in COFF form is not consumed */
        if (0 == strncmp(name, ".debug", 6) || 0 == strncmp(name, ".stab", 5))
            continue;

        if (sel) {
            const char *key = NULL;
            if (sel == COFF_COMDAT_ASSOCIATIVE)
                continue;   /* resolved in pass 3, once the rest are decided */
            if (sel > COFF_COMDAT_EXACT_MATCH) {
                /* IMAGE_COMDAT_SELECT_LARGEST, and anything unknown, would
                   need deferred selection; refuse loudly rather than guess. */
                tcc_error_noabort("PE-COFF object: unsupported COMDAT selection %d"
                                  " for section '%s'", sel, name);
                goto the_end;
            }
            /* SELECT_NODUPLICATES / ANY / SAME_SIZE / EXACT_MATCH are all
               handled as keep-the-first, keyed on the COMDAT symbol - the
               first external symbol defined in the section ([PECOFF] 5.5.5).
               For SAME_SIZE and EXACT_MATCH that differs from a real linker
               only in the diagnostics it would emit, not in what gets
               linked. */
            for (j = 0; j < nsyms; j += 1 + symtab[j * COFF_SYMESZ + SY_NUMAUX]) {
                unsigned char *sy = symtab + j * COFF_SYMESZ;
                if (sy[SY_SCLASS] == COFF_C_EXT
                    && (int16_t)read16le(sy + SY_SCNUM) == i) {
                    key = coff_name(sy + SY_NAME, nbuf2, strtab, strtab_size, 0);
                    break;
                }
            }
            if (key && find_elf_sym(symtab_section, key)) {
                sec[i].dup = 1;
                continue;
            }
        }
        sec[i].keep = 1;
    }
    /* pass 3: an associative COMDAT lives or dies with the section it names */
    for (i = 1; i <= nsec; i++) {
        int assoc;
        if (sec[i].comdat != COFF_COMDAT_ASSOCIATIVE)
            continue;
        if (sec[i].sym < 0) {
            tcc_error_noabort("PE-COFF object: associative COMDAT section %d has"
                              " no section symbol", i);
            goto the_end;
        }
        assoc = read16le(symtab + (sec[i].sym + 1) * COFF_SYMESZ + AUX_SCN_ASSOC);
        if (assoc < 1 || assoc > nsec || sec[assoc].comdat == COFF_COMDAT_ASSOCIATIVE) {
            tcc_error_noabort("PE-COFF object: associative COMDAT section %d refers"
                              " to bad section %d", i, assoc);
            goto the_end;
        }
        sec[i].keep = sec[assoc].keep;
        sec[i].dup = sec[assoc].dup;
    }

    /* --- pass 4: merge the kept sections into tcc's sections ----------- */
    for (i = 1; i <= nsec; i++) {
        unsigned flags, align;
        int sh_type, sh_flags;
        const char *name;
        Section *s = NULL;

        if (!sec[i].keep)
            continue;
        flags = read32le(sec[i].hdr + SH_FLAGS);
        name = coff_name(sec[i].hdr + SH_NAME, nbuf, strtab, strtab_size, 1);
        size = read32le(sec[i].hdr + SH_RAWSIZE);

        sh_type = (flags & IMAGE_SCN_CNT_UNINITIALIZED_DATA)
                  ? SHT_NOBITS : SHT_PROGBITS;
        sh_flags = SHF_ALLOC;
        if (flags & IMAGE_SCN_MEM_WRITE)
            sh_flags |= SHF_WRITE;
        if (flags & IMAGE_SCN_MEM_EXECUTE)
            sh_flags |= SHF_EXECINSTR;
        /* the alignment is a power of two, biased by one, in bits 20..23;
           zero means "unspecified" ([PECOFF] 4.1).  binutils defaults x86
           COFF to 4 bytes (COFF_DEFAULT_SECTION_ALIGNMENT_POWER, [BFD]
           bfd/coff-i386.c). */
        align = (flags & IMAGE_SCN_ALIGN_MASK) >> IMAGE_SCN_ALIGN_SHIFT;
        align = align ? 1u << (align - 1) : 4;

        for (j = 1; j < s1->nb_sections; j++) {
            if (0 == strcmp(s1->sections[j]->name, name)) {
                s = s1->sections[j];
                if (s->sh_type != sh_type) {
                    tcc_error_noabort("section type conflict: %s %02x <> %02x",
                                      name, sh_type, s->sh_type);
                    goto the_end;
                }
                break;
            }
        }
        if (!s) {
            s = new_section(s1, name, sh_type, sh_flags);
            s->sh_addralign = align;
        }
        sec[i].offset = section_add(s, size, align);
        if (align > s->sh_addralign)
            s->sh_addralign = align;
        sec[i].s = s;
        if (sh_type != SHT_NOBITS && size) {
            unsigned long ptr = read32le(sec[i].hdr + SH_RAWPTR);
            if (!ptr) {
                tcc_error_noabort("PE-COFF object: section '%s' has contents but"
                                  " no data pointer", name);
                goto the_end;
            }
            lseek(fd, file_offset + ptr, SEEK_SET);
            if (full_read(fd, s->data + sec[i].offset, size) != (ssize_t)size) {
                tcc_error_noabort("PE-COFF object: truncated contents for section"
                                  " '%s'", name);
                goto the_end;
            }
        }
    }

    /* --- pass 5: symbols ---------------------------------------------- */
    for (i = 0; i < nsyms; ) {
        unsigned char *sy = symtab + i * COFF_SYMESZ;
        int si = i, numaux = sy[SY_NUMAUX];
        int sclass = sy[SY_SCLASS];
        int scnum = (int16_t)read16le(sy + SY_SCNUM);
        addr_t value = read32le(sy + SY_VALUE);
        unsigned long sym_size = 0;
        int bind, type = STT_NOTYPE, shndx;
        const char *name;

        i += 1 + numaux;

        switch (sclass) {
        case COFF_C_EXT: bind = STB_GLOBAL; break;
        case COFF_C_STAT:
        case COFF_C_LABEL: bind = STB_LOCAL; break;
        case COFF_C_NT_WEAK:
        case COFF_C_WEAKEXT: bind = STB_WEAK; break;
        default:
            /* C_FILE, C_SECTION, and the C_BLOCK/C_FCN/C_AUTO/... debugging
               and scoping classes carry no linkage information */
            continue;
        }
        name = coff_name(sy + SY_NAME, nbuf, strtab, strtab_size, 0);
        if (!name || !*name)
            continue;

        if (scnum > 0) {
            if (scnum > nsec) {
                tcc_error_noabort("PE-COFF object: symbol '%s' in bad section %d",
                                  name, scnum);
                goto the_end;
            }
            if (sec[scnum].dup) {
                /* COMDAT duplicate: point references at the copy we kept */
                if (bind != STB_LOCAL)
                    old_to_new[si] = find_elf_sym(symtab_section, name);
                continue;
            }
            if (!sec[scnum].s)
                continue;   /* section not loaded: drop the symbol with it */
            shndx = sec[scnum].s->sh_num;
            value += sec[scnum].offset;
            if (sec[scnum].sym == si)
                type = STT_SECTION;
        } else if (scnum == COFF_N_UNDEF) {
            if (value) {
                /* a COFF common symbol: its size is in n_value ([PECOFF]
                   5.4.4).  ELF puts the size in st_size and the *alignment*
                   in st_value (see resolve_common_syms()).  COFF carries no
                   alignment - GNU as emits it as an "-aligncomm" directive in
                   .drectve, which is not parsed here - so derive a natural
                   alignment from the size, capped at the word size. */
                sym_size = value;
                value = value >= PTR_SIZE ? PTR_SIZE
                      : value >= 4 ? 4 : value >= 2 ? 2 : 1;
                shndx = SHN_COMMON;
            } else {
                shndx = SHN_UNDEF;
                value = 0;
            }
        } else if (scnum == COFF_N_ABS) {
            shndx = SHN_ABS;
        } else {
            continue;   /* N_DEBUG and friends */
        }

        if (bind == STB_WEAK && shndx == SHN_UNDEF)
            tcc_warning("weak external '%s': the default symbol named by its"
                        " auxiliary record is ignored", name);

        old_to_new[si] = set_elf_sym(symtab_section, value, sym_size,
                                     ELFW(ST_INFO)(bind, type), 0, shndx, name);
    }

    /* --- pass 6: relocations ------------------------------------------ */
    for (i = 1; i <= nsec; i++) {
        unsigned long nrel, relptr, vaddr, rawsize;
        unsigned flags;
        const char *name;
        Section *s = sec[i].s;

        if (!s)
            continue;
        flags = read32le(sec[i].hdr + SH_FLAGS);
        nrel = read16le(sec[i].hdr + SH_NRELOC);
        relptr = read32le(sec[i].hdr + SH_RELPTR);
        vaddr = read32le(sec[i].hdr + SH_VADDR);
        rawsize = read32le(sec[i].hdr + SH_RAWSIZE);
        name = coff_name(sec[i].hdr + SH_NAME, nbuf, strtab, strtab_size, 1);
        if (!nrel || !relptr)
            continue;
        if (flags & IMAGE_SCN_LNK_NRELOC_OVFL) {
            /* more than 0xffff relocations: the real count sits in the
               VirtualAddress field of a leading dummy record ([PECOFF] 4.1,
               IMAGE_SCN_LNK_NRELOC_OVFL, and 5.3) */
            unsigned char ovfl[COFF_RELSZ];
            if (!read_mem(fd, file_offset + relptr, ovfl, COFF_RELSZ)
                || !read32le(ovfl + RE_VADDR)) {
                tcc_error_noabort("PE-COFF object: bad relocation overflow record"
                                  " for '%s'", name);
                goto the_end;
            }
            nrel = read32le(ovfl + RE_VADDR) - 1;
            relptr += COFF_RELSZ;
        }
        relocs = load_data(fd, file_offset + relptr, nrel * COFF_RELSZ);
        for (j = 0; j < (int)nrel; j++) {
            unsigned char *re = relocs + j * COFF_RELSZ;
            unsigned long rva = read32le(re + RE_VADDR), off;
            unsigned long symndx = read32le(re + RE_SYMNDX);
            int coff_type = read16le(re + RE_TYPE);
            unsigned char *ptr;
            addr_t addend;
            int bias, fsize, elf_type, sym_index;

            /* r_vaddr is the section's own address plus the offset of the
               item within it ([PECOFF] 5.3); object sections have address 0,
               but subtract it rather than assume. */
            if (rva < vaddr || rva - vaddr > rawsize) {
                tcc_error_noabort("PE-COFF object: relocation for '%s' at 0x%lx is"
                                  " outside the section", name, rva);
                goto the_end;
            }
            off = rva - vaddr;
            ptr = s->sh_type != SHT_NOBITS ? s->data + sec[i].offset + off : NULL;

            elf_type = coff_reloc_type(coff_type, ptr, &addend, &bias, &fsize);
            if (elf_type < 0) {
                /* Loud, by design.  An untranslated type is never passed
                   through and never skipped. */
                tcc_error_noabort("unsupported PE-COFF relocation type %d (0x%02x)"
                                  " in section '%s' at offset 0x%lx",
                                  coff_type, coff_type, name, off);
                goto the_end;
            }
            if ((unsigned long)fsize > rawsize - off) {
                tcc_error_noabort("PE-COFF object: relocation for '%s' at 0x%lx"
                                  " overruns the section", name, rva);
                goto the_end;
            }
            if (symndx >= (unsigned long)nsyms) {
                tcc_error_noabort("PE-COFF object: relocation in '%s' names symbol"
                                  " %lu of %d", name, symndx, nsyms);
                goto the_end;
            }
            sym_index = old_to_new[symndx];
            if (!sym_index && elf_type != R_XXX_NONE) {
                tcc_error_noabort("invalid relocation entry in '%s' @ 0x%lx",
                                  name, off);
                goto the_end;
            }
            off += sec[i].offset;
            if (fsize && ptr) {
                if (bias)               /* REL target: fold the bias in place */
                    add32le(ptr, bias);
                if (SHT_RELX == SHT_RELA)  /* RELA: the addend moved out */
                    memset(ptr, 0, fsize);
            }
            put_elf_reloca(symtab_section, s, off, elf_type, sym_index, addend);
        }
        tcc_free(relocs), relocs = NULL;
    }

    ret = !s1->nb_errors - 1;
 the_end:
    tcc_free(relocs);
    tcc_free(old_to_new);
    tcc_free(sec);
    tcc_free(strtab);
    tcc_free(symtab);
    tcc_free(shdrs);
    return ret;
}

/* ------------------------------------------------------------- */
PUB_FUNC int tcc_get_dllexports(const char *filename, char **pp)
{
    int ret, fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0)
        return -1;
    ret = get_dllexports(fd, pp);
    close(fd);
    return ret;
}

/* ------------------------------------------------------------- */
#ifdef TCC_TARGET_X86_64
static unsigned pe_add_unwind_info(TCCState *s1)
{
    if (NULL == s1->uw_pdata) {
        s1->uw_pdata = find_section(s1, ".pdata");
        s1->uw_pdata->sh_addralign = 4;
    }
    if (0 == s1->uw_sym)
        s1->uw_sym = put_elf_sym(symtab_section, 0, 0, 0, 0, text_section->sh_num, ".uw_base");
    if (0 == s1->uw_offs) {
        /* As our functions all have the same stackframe, we use one entry for all */
        static const unsigned char uw_info[] = {
            0x01, // UBYTE: 3 Version , UBYTE: 5 Flags
            0x04, // UBYTE Size of prolog
            0x02, // UBYTE Count of unwind codes
            0x05, // UBYTE: 4 Frame Register (rbp), UBYTE: 4 Frame Register offset (scaled)
            // USHORT * n Unwind codes array (descending order)
            // 0x0b, 0x01, 0xff, 0xff, // stack size
            // UBYTE offset of end of instr in prolog + 1, UBYTE:4 operation, UBYTE:4 info
            0x04, 0x03, // 3:0 UWOP_SET_FPREG (mov rsp -> rbp)
            0x01, 0x50, // 0:5 UWOP_PUSH_NONVOL (push rbp)
        };

        Section *s = text_section;
        unsigned char *p;

        section_ptr_add(s, -s->data_offset & 3); /* align */
        s1->uw_offs = s->data_offset;
        p = section_ptr_add(s, sizeof uw_info);
        memcpy(p, uw_info, sizeof uw_info);
    }

    return s1->uw_offs;
}

ST_FUNC void pe_add_unwind_data(unsigned start, unsigned end, unsigned stack)
{
    TCCState *s1 = tcc_state;
    Section *pd;
    unsigned o, n, d;
    struct /* _RUNTIME_FUNCTION */ {
      DWORD BeginAddress;
      DWORD EndAddress;
      DWORD UnwindData;
    } *p;

    d = pe_add_unwind_info(s1);
    pd = s1->uw_pdata;
    o = pd->data_offset;
    p = section_ptr_add(pd, sizeof *p);

    /* record this function */
    p->BeginAddress = start;
    p->EndAddress = end;
    p->UnwindData = d;

    /* put relocations on it */
    for (n = o + sizeof *p; o < n; o += sizeof p->BeginAddress)
        put_elf_reloc(symtab_section, pd, o, R_XXX_RELATIVE, s1->uw_sym);
}

#elif defined(TCC_TARGET_ARM64)
/* ARM64 unwind codes:
   save_fplr_x: 10iiiiii  - stp x29,lr,[sp,#-(i+1)*8]!
   set_fp:      11100001  - mov x29,sp
   alloc_s:     000iiiii  - sub sp,sp,#i*16 (up to 496 bytes)
   alloc_m:     11000iii xxxxxxxx - sub sp,sp,#X*16 (up to 32KB)
   end:         11100100  - end of unwind codes
*/
static Section *pe_add_unwind_info(TCCState *s1)
{
    Section *s;

    if (NULL == s1->uw_pdata) {
        s1->uw_pdata = find_section(s1, ".pdata");
        s1->uw_pdata->sh_addralign = 4;
    }
    s = find_section(s1, ".xdata");
    s->sh_addralign = 4;
    if (0 == s1->uw_sym)
        s1->uw_sym = put_elf_sym(symtab_section, 0, 0, 0, 0,
                                  text_section->sh_num, ".uw_text_base");
    if (0 == s1->uw_xsym)
        s1->uw_xsym = put_elf_sym(symtab_section, 0, 0, 0, 0,
                                  s->sh_num, ".uw_base");
    return s;
}

ST_FUNC void pe_add_unwind_data(unsigned start, unsigned end, unsigned stack)
{
    TCCState *s1 = tcc_state;
    Section *pd, *xd;
    unsigned o, d, code_bytes, func_len;
    unsigned char *q;
    uint32_t header;
    struct /* _RUNTIME_FUNCTION */ {
        DWORD BeginAddress;
        DWORD UnwindData;
    } *p;

    int epilog;

    xd = pe_add_unwind_info(s1);
    pd = s1->uw_pdata;

    func_len = (end - start) >> 2;
    code_bytes = 0;
    epilog = code_bytes;
    code_bytes += 3; /* set_fp, save_fplr_x, end */
    code_bytes = (code_bytes + 3) & ~3;

    section_ptr_add(xd, -xd->data_offset & 3);
    d = xd->data_offset;
    q = section_ptr_add(xd, 4 + code_bytes);

    /* Full ARM64 xdata header: E=1 with one epilog and no exception handler. */
    header = (func_len & 0x3ffff)
        | 1 << 21
        | (epilog & 0x1F) << 22
        | (code_bytes >> 2) << 27
        ;
    write32le(q, header);
    q += 4;
    *q++ = 0xE1; /* set_fp */
    *q++ = 0x9B; /* save_fplr_x: stp x29,lr,[sp,#-224]! */
    *q++ = 0xE4; /* end */
    while ((unsigned)(q - (xd->data + d + 4)) < code_bytes)
        *q++ = 0xE3; /* nop padding */

    o = pd->data_offset;
    p = section_ptr_add(pd, sizeof *p);
    p->BeginAddress = start;
    p->UnwindData = d;
    put_elf_reloc(symtab_section, pd, o, R_XXX_RELATIVE, s1->uw_sym);
    put_elf_reloc(symtab_section, pd, o + 4, R_XXX_RELATIVE, s1->uw_xsym);
}
#endif
/* ------------------------------------------------------------- */
#if defined(TCC_TARGET_X86_64) || defined(TCC_TARGET_ARM64)
#define PE_STDSYM(n,s) n
#else
#define PE_STDSYM(n,s) "_" n s
#endif

static void pe_add_runtime(TCCState *s1, struct pe_info *pe)
{
    const char *start_symbol;
    int pe_type;

    if (TCC_OUTPUT_DLL == s1->output_type) {
        pe_type = PE_DLL;
        start_symbol = PE_STDSYM("__dllstart","@12");
    } else {
        const char *run_symbol;
        if (find_elf_sym(symtab_section, PE_STDSYM("WinMain","@16"))) {
            start_symbol = "__winstart";
            run_symbol = "__runwinmain";
            pe_type = PE_GUI;
        } else if (find_elf_sym(symtab_section, PE_STDSYM("wWinMain","@16"))) {
            start_symbol = "__wwinstart";
            run_symbol = "__runwwinmain";
            pe_type = PE_GUI;
        } else if (find_elf_sym(symtab_section, "wmain")) {
            start_symbol = "__wstart";
            run_symbol = "__runwmain";
            pe_type = PE_EXE;
        } else {
            start_symbol =  "__start";
            run_symbol = "__runmain";
            pe_type = PE_EXE;
            if (s1->pe_subsystem == 2)
                pe_type = PE_GUI;
        }

        if (TCC_OUTPUT_MEMORY == s1->output_type && !s1->nostdlib)
            start_symbol = run_symbol;
    }
    if (s1->elf_entryname) {
        pe->start_symbol = start_symbol = s1->elf_entryname;
    } else {
        pe->start_symbol = start_symbol + 1;
        if (!s1->leading_underscore || strchr(start_symbol, '@'))
            ++start_symbol;
    }

#ifdef CONFIG_TCC_BACKTRACE
    if (s1->do_backtrace) {
#ifdef CONFIG_TCC_BCHECK
        if (s1->do_bounds_check && s1->output_type != TCC_OUTPUT_DLL)
            tcc_add_support(s1, "bcheck.o");
#endif
        if (s1->output_type == TCC_OUTPUT_EXE)
            tcc_add_support(s1, "bt-exe.o");
        if (s1->output_type == TCC_OUTPUT_DLL)
            tcc_add_support(s1, "bt-dll.o");
        if (s1->output_type != TCC_OUTPUT_DLL)
            tcc_add_support(s1, "bt-log.o");
        tcc_add_btstub(s1);
    }
#endif

    /* grab the startup code from libtcc1.a */
#ifdef TCC_IS_NATIVE
    if (TCC_OUTPUT_MEMORY != s1->output_type || s1->run_main)
#endif
    set_global_sym(s1, start_symbol, NULL, 0);

    if (0 == s1->nostdlib) {
        static const char * const libs[] = {
            "msvcrt", "kernel32", "", "user32", "gdi32", NULL
        };
        const char * const *pp, *p;
        if (TCC_LIBTCC1[0])
            tcc_add_support(s1, TCC_LIBTCC1);
        s1->static_link = 0; /* no static crt for tcc */
        for (pp = libs; 0 != (p = *pp); ++pp) {
            if (*p)
                tcc_add_library(s1, p);
            else if (PE_DLL != pe_type && PE_GUI != pe_type)
                break;
        }
    }

    /* need this for 'tccelf.c:relocate_sections()' */
    if (TCC_OUTPUT_DLL == s1->output_type)
        s1->output_type = TCC_OUTPUT_EXE;
    if (TCC_OUTPUT_MEMORY == s1->output_type)
        pe_type = PE_RUN;
    pe->type = pe_type;
}

ST_FUNC int pe_setsubsy(TCCState *s1, const char *arg)
{
    static const struct subsy { const char* p; int v; } x[] = {
#if defined(TCC_TARGET_I386) || defined(TCC_TARGET_X86_64) || defined(TCC_TARGET_ARM64)
        { "native", 1 },
        { "gui", 2 },
        { "windows", 2 },
        { "console", 3 },
        { "posix", 7 },
        { "efiapp", 10 },
        { "efiboot", 11 },
        { "efiruntime", 12 },
        { "efirom", 13 },
#elif defined(TCC_TARGET_ARM)
        { "wince", 9 },
#endif
        { 0, -1 }};
    const struct subsy *y;
    for (y = x;; ++y) {
        if (!y->p)
            return -1;
        if (0 == strcmp(y->p, arg)) {
            s1->pe_subsystem = y->v;
            return 0;
        }
    }
}

static void pe_set_options(TCCState * s1, struct pe_info *pe)
{
    if (PE_DLL == pe->type) {
        /* XXX: check if is correct for arm-pe target */
        pe->imagebase = IMAGE_BASE_DLL;
    } else {
        pe->imagebase = IMAGE_BASE_EXE;
    }

#if defined(TCC_TARGET_ARM)
    /* we use "console" subsystem by default */
    pe->subsystem = 9;
#else
    if (PE_DLL == pe->type || PE_GUI == pe->type)
        pe->subsystem = 2;
    else
        pe->subsystem = 3;
#endif
    /* Allow override via -Wl,-subsystem=... option */
    if (s1->pe_subsystem != 0)
        pe->subsystem = s1->pe_subsystem;

    /* set default file/section alignment */
    if (pe->subsystem == 1) {
        /* unmeasured: whether a native image at 0x20 maps through the
           kernel path that actually loads native images. CreateProcess
           rejects it, but rejects well-formed native images too. */
        pe->section_align = 0x20;
        pe->file_align = 0x20;
    } else {
        pe->section_align = 0x1000;
        pe->file_align = 0x200;
    }

    if (s1->section_align != 0)
        pe->section_align = s1->section_align;
    if (s1->pe_file_align != 0)
        pe->file_align = s1->pe_file_align;

    if ((pe->subsystem >= 10) && (pe->subsystem <= 12))
        pe->imagebase = 0;

    if (s1->has_text_addr)
        pe->imagebase = s1->text_addr;
}

ST_FUNC int pe_output_file(TCCState *s1, const char *filename)
{
    struct pe_info pe;

    memset(&pe, 0, sizeof pe);
    pe.filename = filename;
    pe.s1 = s1;
    s1->filetype = 0;

#ifdef CONFIG_TCC_BCHECK
    tcc_add_bcheck(s1);
#endif
    tcc_add_pragma_libs(s1);
    pe_add_runtime(s1, &pe);
    resolve_common_syms(s1);
    pe_set_options(s1, &pe);
    pe_check_symbols(&pe);
    if (s1->nb_errors)
        goto done;
    if (filename) {
        pe_assign_addresses(&pe);
        relocate_syms(s1, s1->symtab, 0);
        if (s1->nb_errors)
            goto done;
        s1->pe_imagebase = pe.imagebase;
        relocate_sections(s1);
        pe.start_addr = (DWORD)
            (get_sym_addr(s1, pe.start_symbol, 1, 1) - pe.imagebase);
        if (s1->nb_errors)
            goto done;
        pe_write(&pe);
    } else {
        /* -run */
#ifdef TCC_IS_NATIVE
        pe.thunk = data_section;
        pe_build_imports(&pe);
        s1->run_main = pe.start_symbol;
#if defined(TCC_TARGET_X86_64) || defined(TCC_TARGET_ARM64)
        s1->uw_pdata = find_section(s1, ".pdata");
#endif
#endif
    }
done:
    dynarray_reset(&pe.sec_info, &pe.sec_count);
    pe_free_imports(&pe);
#if PE_PRINT_SECTIONS
    if (g_debug & 8)
        pe_print_sections(s1, "tcc.log");
#endif
    return s1->nb_errors ? -1 : 0;
}

/* ------------------------------------------------------------- */
