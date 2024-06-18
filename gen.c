#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
  // First things first, we need to allocate a buffer with enough space.
  const size_t bufferSize = 1024;
  uint8_t *buffer = malloc(bufferSize);
  memset(buffer, 0, bufferSize);

  // And since we will need to keep track of its current length,
  // lets actually keep it.
  off_t offset = 0;

  // Every Mach-O file has a header file describing the file.
  // Lets fill it in for a 64-bit linkable ARM object file.
  struct mach_header_64 *header = (struct mach_header_64 *)buffer;
  offset += sizeof(*header);
  header->magic = MH_MAGIC_64;
  header->cputype = CPU_TYPE_ARM64;
  header->cpusubtype = CPU_SUBTYPE_ARM64_ALL;
  header->filetype = MH_OBJECT;
  header->ncmds = 0;
  header->sizeofcmds = 0;
  header->flags = 0;
  header->reserved = 0;

  // Next up we will define a text segment and section.
  // Not quite sure why we need both (unlike ELF) but we do.
  // nevertheless, lets fill them in.
  struct segment_command_64 *textSegment;
  struct section_64 *textSection;
  {
    textSegment = (struct segment_command_64 *)(buffer + offset);
    offset += sizeof(*textSegment);
    textSegment->cmd = LC_SEGMENT_64;
    textSegment->cmdsize = sizeof(*textSegment);
    memset(textSegment->segname, 0, sizeof(textSegment->segname));
    textSegment->vmaddr = 0;
    textSegment->vmsize = 0;
    textSegment->fileoff = 0;
    textSegment->filesize = 0;
    textSegment->maxprot = VM_PROT_ALL;
    textSegment->initprot = VM_PROT_ALL;
    textSegment->nsects = 0;
    textSegment->flags = 0;

    textSection = (struct section_64 *)(buffer + offset);
    offset += sizeof(*textSection);
    strcpy(textSection->sectname, SECT_TEXT);
    strcpy(textSection->segname, SEG_TEXT);
    textSection->addr = 0;
    textSection->size = 0;
    textSection->offset = 0;
    textSection->align = 4;
    textSection->reloff = 0;
    textSection->nreloc = 0;
    textSection->flags = S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS;
    textSection->reserved1 = 0;
    textSection->reserved2 = 0;
    textSection->reserved3 = 0;

    // Don't forget to update the segment.
    // Dude has to know about its sections.
    // Like imagine being a father without knowing how many kids you have.
    // Even tho they are right next to you you just don't know
    // how many is yours unless the b**ch tells you.
    // That'd be a sad life.
    textSegment->nsects++;
    textSegment->cmdsize += sizeof(*textSection);

    // Obviously gotta add the segment to the header.
    // Same issue different level.
    header->ncmds++;
    header->sizeofcmds += textSegment->cmdsize;
  }

  {
    // I don't really know why this one is particularly needed.
    // I think it's something about the version of the compiler.
    // Not sure tho.
    struct build_version_command *buildVersion =
        (struct build_version_command *)(buffer + offset);
    offset += sizeof(*buildVersion);
    buildVersion->cmd = LC_BUILD_VERSION;
    buildVersion->cmdsize = sizeof(*buildVersion);
    buildVersion->platform = PLATFORM_MACOS;
    buildVersion->minos = 0xf0000;
    buildVersion->sdk = 0;
    buildVersion->ntools = 0;

    // Add it to the header.
    header->ncmds++;
    header->sizeofcmds += buildVersion->cmdsize;
  }

  // SYMBOL TABLE BABYYYYY
  // YEAH!!
  struct symtab_command *symtab = (struct symtab_command *)(buffer + offset);
  offset += sizeof(*symtab);
  {
    symtab->cmd = LC_SYMTAB;
    symtab->cmdsize = sizeof(*symtab);
    symtab->symoff = 0;
    symtab->nsyms = 0;
    symtab->stroff = 0;
    symtab->strsize = 0;

    header->ncmds++;
    header->sizeofcmds += symtab->cmdsize;
  }

  // dynamic symbol table
  // meh..
  struct dysymtab_command *dysymtab =
      (struct dysymtab_command *)(buffer + offset);
  offset += sizeof(*dysymtab);
  {
    dysymtab->cmd = LC_DYSYMTAB;
    dysymtab->cmdsize = sizeof(*dysymtab);
    dysymtab->ilocalsym = 0;
    dysymtab->nlocalsym = 0;
    dysymtab->iextdefsym = 0;
    dysymtab->nextdefsym = 0;
    dysymtab->iundefsym = 0;
    dysymtab->nundefsym = 0;
    dysymtab->tocoff = 0;
    dysymtab->ntoc = 0;
    dysymtab->modtaboff = 0;
    dysymtab->nmodtab = 0;
    dysymtab->extrefsymoff = 0;
    dysymtab->nextrefsyms = 0;
    dysymtab->indirectsymoff = 0;
    dysymtab->nindirectsyms = 0;
    dysymtab->extreloff = 0;
    dysymtab->nextrel = 0;
    dysymtab->locreloff = 0;
    dysymtab->nlocrel = 0;

    header->ncmds++;
    header->sizeofcmds += dysymtab->cmdsize;
  }

  // Now we're getting to the spicy part.
  {
    // You see we gotta go back here and fix the text segment/section.
    // Because we didn't tell the offset of where the instructions actually
    // begin.
    uint32_t *textData = (uint32_t *)(buffer + offset);
    textSegment->fileoff = offset;
    textSection->offset = offset;

    // After doing that we can just raw-dog the instructions
    // like real men do.
    *(textData++) = 0xd2800000; // mov x0, #0       ; return code
    *(textData++) = 0xd2800030; // mov x16, #1      ; exit syscall
    *(textData++) = 0xd4001001; // svc #0xd2800000  ; go dance baby

    // Now that we know how much space the instructions take
    // lets go back and fix the size stuff for the segment and section.
    int size = (textData - (uint32_t *)(buffer + offset)) * 4;
    textSegment->vmsize = size;
    textSegment->filesize = size;
    textSection->size = textSegment->filesize;
    offset += size;
  }

  // NOTICE:
  // Okay the hard part is done.
  // We have a valid Mach-O file with a revolutionary program in it.
  // All we gotta do is add a "_main" symbol, bind it to where the instructions
  // begin in the file and bam. Done.
  //
  // Lets first tell people about which offset
  // we keep or symbols at tho.
  offset += 4;
  symtab->symoff = offset;

  // Add a "_main" symbol
  {
    struct nlist_64 *main_entry = (struct nlist_64 *)(buffer + offset);
    offset += sizeof(*main_entry);
    main_entry->n_un.n_strx = 1;
    main_entry->n_type = N_EXT | N_SECT;
    main_entry->n_sect = 1;
    main_entry->n_desc = 0;
    main_entry->n_value = 0;
    symtab->nsyms++;
  }

  // Adjust dysymtab
  dysymtab->nextdefsym = symtab->nsyms;
  dysymtab->iundefsym = dysymtab->iextdefsym + dysymtab->nextdefsym;

  // Now we made the entry for "_main",
  // but we gon need the strings table for it too.
  // Or else how tf would the linker know what its name is?
  char *strtbl = (char *)(buffer + offset);
  strcpy(strtbl + 1, "_start");
  symtab->stroff = offset;
  symtab->strsize = strlen(strtbl + 1) + 1;
  offset += symtab->strsize;

  // Save that shit brother I'm outta here.
  FILE *file = fopen("hello.o", "wb");
  fwrite(buffer, offset, 1, file);
  fclose(file);
  free(buffer);
}
