/*
dfuse.{c,h} :
Defines the data structures that follow the DfuSe file format. Has routines for
reading and writing each of the different sections of a DfuSe file.

More information on the DfuSe file format is available in DfuSe File Format
Specification, UM0391.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "dfuse.h"
#include "crc32.h"

// static const char *STMJUNK =
//     "ababababababababababababababababababababababababababababababababababab"
//     "ababababababababababababababababababababababababababababababababababab"
//     "ababababababababababababababababababababababababababababababababababab"
//     "ababababababababababababababababababababababab";

#define HI(x) (x >> 8 & 0xFF)
#define LO(x) (x & 0xFF)

/*
    dfuse_init() allocates memory for the various dfuse structs
    that make up the dfuse file, and populates fields that are
    independent of the firmware image.
*/
dfuse_file *dfuse_init(uint32_t device_id, uint32_t vendor_id,
                       uint32_t product_id)
{
    // allocate memory
    dfuse_file *dfusefile = (dfuse_file *)malloc(sizeof(dfuse_file));
    dfusefile->prefix = (dfuse_prefix *)malloc(sizeof(dfuse_prefix));
    dfusefile->prefix->targets = 0;
    dfusefile->prefix->signature[0] = 'D';
    dfusefile->prefix->signature[1] = 'f';
    dfusefile->prefix->signature[2] = 'u';
    dfusefile->prefix->signature[3] = 'S';
    dfusefile->prefix->signature[4] = 'e';
    dfusefile->prefix->version = 0x01;
    dfusefile->prefix->dfu_image_size = STMDFU_PREFIXLEN;

    dfusefile->images = NULL;
    dfusefile->suffix = (dfuse_suffix *)malloc(sizeof(dfuse_suffix));

    // set predetermined prefix values
    // set predetermined suffix values
    dfusefile->suffix->device_low = LO(device_id);
    dfusefile->suffix->device_high = HI(device_id);
    dfusefile->suffix->product_low = LO(product_id);
    dfusefile->suffix->product_high = HI(product_id);
    dfusefile->suffix->vendor_low = LO(vendor_id);
    dfusefile->suffix->vendor_high = HI(vendor_id);
    dfusefile->suffix->dfu_low = 0x1a;
    dfusefile->suffix->dfu_high = 0x01;
    dfusefile->suffix->dfu_signature[0] = 'U';
    dfusefile->suffix->dfu_signature[1] = 'F';
    dfusefile->suffix->dfu_signature[2] = 'D';
    dfusefile->suffix->suffix_length = 16;

    return dfusefile;
}

dfuse_image *dfuse_addimage(dfuse_file *dfusefile, const char *target_name,
                            uint8_t alternate_setting)
{
    dfusefile->prefix->targets++;
    if (dfusefile->images) {
        dfusefile->images = (dfuse_image **)realloc(
            dfusefile->images,
            sizeof(dfuse_image *) * dfusefile->prefix->targets);

    } else {
        dfusefile->images = (dfuse_image **)malloc(sizeof(dfuse_image *) *
                                                   dfusefile->prefix->targets);
    }

    dfuse_image *image = (dfuse_image *)malloc(sizeof(dfuse_image));
    dfusefile->images[dfusefile->prefix->targets - 1] = image;

    image->imgelement = NULL;

    image->tarprefix =
        (dfuse_target_prefix *)malloc(sizeof(dfuse_target_prefix));
    image->tarprefix->num_elements = 0;
    image->tarprefix->signature[0] = 'T';
    image->tarprefix->signature[1] = 'a';
    image->tarprefix->signature[2] = 'r';
    image->tarprefix->signature[3] = 'g';
    image->tarprefix->signature[4] = 'e';
    image->tarprefix->signature[5] = 't';
    image->tarprefix->alternate_setting = alternate_setting;
    image->tarprefix->target_named = 1;

    memset(image->tarprefix->target_name, 0,
           sizeof(image->tarprefix->target_name));
    strcpy(image->tarprefix->target_name, target_name);
    image->tarprefix->target_size = 0;

    dfusefile->prefix->dfu_image_size += STMDFU_TARPREFIXLEN;

    return image;
}

dfuse_image_element *dfuse_addelement(dfuse_file *dfusefile, dfuse_image *image,
                                      unsigned int address, int size)
{
    image->tarprefix->num_elements++;
    if (image->imgelement) {
        image->imgelement = (dfuse_image_element **)realloc(
            image->imgelement,
            sizeof(dfuse_image_element *) * image->tarprefix->num_elements);

    } else {
        image->imgelement = (dfuse_image_element **)malloc(
            sizeof(dfuse_image_element *) * image->tarprefix->num_elements);
    }

    dfuse_image_element *el =
        (dfuse_image_element *)malloc(sizeof(dfuse_image_element));
    image->imgelement[image->tarprefix->num_elements - 1] = el;

    el->element_address = address;
    el->element_size = size;
    el->data = (uint8_t *)malloc(size);

    int delta_size =
        size + sizeof(el->element_address) + sizeof(el->element_size);
    image->tarprefix->target_size += delta_size;
    dfusefile->prefix->dfu_image_size += delta_size;

    return el;
}

/*
        dfuse_readbin() reads the binary firmware image into memory
*/
void dfuse_readbin(dfuse_file *dfusefile, dfuse_image *image, int binfile)
{
    struct stat stat;
    fstat(binfile, &stat);

    dfuse_image_element *el =
        dfuse_addelement(dfusefile, image, 0x08000000, stat.st_size);

    int i, j;

    // read binary into data array
    i = read(binfile, &el->data[0], READBIN_READLEN);

    // when read returns < READBIN_READLEN, we've hit EOF on previous read (or
    // error...)
    for (j = i; i == READBIN_READLEN; j += READBIN_READLEN) {
        i = read(binfile, &el->data[j], READBIN_READLEN);
    }
}

int dfuse_writeprefix(dfuse_file *dfusefile, int dfufile)
{
    int ct = 0;

    ct = DFUWRITE(dfusefile->prefix->signature);
    ct += DFUWRITE(dfusefile->prefix->version);
    ct += DFUWRITE(dfusefile->prefix->dfu_image_size);
    ct += DFUWRITE(dfusefile->prefix->targets);

    if (ct != STMDFU_PREFIXLEN)
        ct = -1;

    return ct;
}

int dfuse_readprefix(dfuse_file *dfusefile, int dfufile)
{
    int ct = 0;

    ct = DFUREAD(dfusefile->prefix->signature);
    ct += DFUREAD(dfusefile->prefix->version);
    ct += DFUREAD(dfusefile->prefix->dfu_image_size);
    ct += DFUREAD(dfusefile->prefix->targets);

    if (ct != STMDFU_PREFIXLEN)
        ct = -1;

    return ct;
}

int dfuse_writetarprefix(dfuse_image *image, int dfufile)
{
    int ct = 0;

    ct = DFUWRITE(image->tarprefix->signature);
    ct += DFUWRITE(image->tarprefix->alternate_setting);
    ct += DFUWRITE(image->tarprefix->target_named);
    ct += DFUWRITE(image->tarprefix->target_name);
    ct += DFUWRITE(image->tarprefix->target_size);
    ct += DFUWRITE(image->tarprefix->num_elements);

    if (ct != STMDFU_TARPREFIXLEN)
        ct = -1;

    return ct;
}

int dfuse_readtarprefix(dfuse_image *image, int dfufile)
{
    int ct = 0;

    ct = DFUREAD(image->tarprefix->signature);
    ct += DFUREAD(image->tarprefix->alternate_setting);
    ct += DFUREAD(image->tarprefix->target_named);
    ct += DFUREAD(image->tarprefix->target_name);
    ct += DFUREAD(image->tarprefix->target_size);
    ct += DFUREAD(image->tarprefix->num_elements);

    if (ct != STMDFU_TARPREFIXLEN)
        ct = -1;

    return ct;
}

int dfuse_writeimages(dfuse_file *dfusefile, int dfufile)
{
    int ct = 0;
    for (int i = 0; i < dfusefile->prefix->targets; i++) {
        dfuse_image *image = dfusefile->images[i];
        ct += dfuse_writetarprefix(image, dfufile);
        for (int j = 0; j < image->tarprefix->num_elements; j++) {
            dfuse_image_element *el = image->imgelement[j];
            ct += dfuse_writeimgelement(el, dfufile);
        }
    }
    return ct;
}

int dfuse_writeimgelement(dfuse_image_element *el, int dfufile)
{
    int ct = 0;

    ct += DFUWRITE(el->element_address);
    ct += DFUWRITE(el->element_size);
    ct += write(dfufile, el->data, el->element_size);
    if (ct != el->element_size + sizeof(el->element_address) +
                  sizeof(el->element_size)) {
        ct = -1;
    }

    return ct;
}

int dfuse_readimgelement_meta(dfuse_image_element *el, int dfufile)
{
    int ct = 0;

    ct += DFUREAD(el->element_address);
    ct += DFUREAD(el->element_size);

    if (ct != sizeof(el->element_address) + sizeof(el->element_size)) {
        ct = -1;
    }

    return ct;
}

int dfuse_readimgelement_data(dfuse_image_element *el, int dfufile)
{
    int ct = 0;

    ct += read(dfufile, el->data, el->element_size);

    if (ct != el->element_size) {
        ct = -1;
    }

    return ct;
}

int dfuse_writesuffix(dfuse_file *dfusefile, int dfufile)
{
    int ct = 0;

    ct = DFUWRITE(dfusefile->suffix->device_low);
    ct += DFUWRITE(dfusefile->suffix->device_high);
    ct += DFUWRITE(dfusefile->suffix->product_low);
    ct += DFUWRITE(dfusefile->suffix->product_high);
    ct += DFUWRITE(dfusefile->suffix->vendor_low);
    ct += DFUWRITE(dfusefile->suffix->vendor_high);
    ct += DFUWRITE(dfusefile->suffix->dfu_low);
    ct += DFUWRITE(dfusefile->suffix->dfu_high);
    ct += DFUWRITE(dfusefile->suffix->dfu_signature);
    ct += DFUWRITE(dfusefile->suffix->suffix_length);

    calccrc(dfusefile, dfufile);

    ct += DFUWRITE(dfusefile->suffix->crc);

    if (ct != STMDFU_SUFFIXLEN)
        ct = -1;

    return ct;
}

int dfuse_readsuffix(dfuse_file *dfusefile, int dfufile)
{
    int ct = 0;

    ct = DFUREAD(dfusefile->suffix->device_low);
    ct += DFUREAD(dfusefile->suffix->device_high);
    ct += DFUREAD(dfusefile->suffix->product_low);
    ct += DFUREAD(dfusefile->suffix->product_high);
    ct += DFUREAD(dfusefile->suffix->vendor_low);
    ct += DFUREAD(dfusefile->suffix->vendor_high);
    ct += DFUREAD(dfusefile->suffix->dfu_low);
    ct += DFUREAD(dfusefile->suffix->dfu_high);
    ct += DFUREAD(dfusefile->suffix->dfu_signature);
    ct += DFUREAD(dfusefile->suffix->suffix_length);
    ct += DFUREAD(dfusefile->suffix->crc);

    if (ct != STMDFU_SUFFIXLEN)
        ct = -1;

    return ct;
}

/*
        calccrc() calculates a 32 bit CRC to go in the
        suffix of the dfuse file
*/
void calccrc(dfuse_file *dfusefile, int dfufile)
{
    unsigned char *crcbuf;

    chksum_crc32gentab();

    // 	int offset1 = 0;
    // 	int offset2 = STMDFU_PREFIXLEN - 1;
    // 	int offset3 = STMDFU_PREFIXLEN + STMDFU_TARPREFIXLEN - 1;
    // 	int offset4 = STMDFU_PREFIXLEN + STMDFU_TARPREFIXLEN + 8 - 1;
    //
    // 	lseek(dfufile, 0, SEEK_SET);
    // 	crcbuf = (char *)malloc(sizeof(char)*fptr);
    // 	if (fptr != read(dfufile, crcbuf, fptr))
    // 		printf("UHOHHHHH\n");
    // 	lseek(dfufile, 0, SEEK_END);
    // 	u_int32_t crc1 = chksum_crc32(&crcbuf[offset1], (fptr-offset1));
    // 	u_int32_t crc2 = chksum_crc32(&crcbuf[offset2], (fptr-offset2+1));
    // 	u_int32_t crc3 = chksum_crc32(&crcbuf[offset3], (fptr-offset3+1));
    // 	u_int32_t crc4 = chksum_crc32(&crcbuf[offset4], (fptr-offset4+1));
    // 	dfusefile->suffix->crc = 13;
    // 	free(crcbuf);
    //
    // 	printf("<%x>%d :: <%x>%d :: <%x>%d :: <%x>%d of %d\n", crc1, offset1,
    // crc2, offset2, crc3, offset3, crc4, offset4, fptr);

    lseek(dfufile, 0, SEEK_SET);

    crcbuf =
        (unsigned char *)malloc(sizeof(char) * (dfusefile->prefix->dfu_image_size + 12));
    if ((dfusefile->prefix->dfu_image_size + 12) !=
        read(dfufile, crcbuf, (dfusefile->prefix->dfu_image_size + 12)))
        printf("UHOHHHHHHHH\n");
    lseek(dfufile, 0, SEEK_END);
    dfusefile->suffix->crc =
        chksum_crc32(crcbuf, (dfusefile->prefix->dfu_image_size + 12));
    free(crcbuf);
}

/*
        dfuse_struct_cleanup() deallocates the dfuse file
        structures
*/
void dfuse_struct_cleanup(dfuse_file *dfusefile)
{
    int i, j;

    for (i = 0; i < dfusefile->prefix->targets; i++) {
        for (j = 0; j < dfusefile->images[i]->tarprefix->num_elements; j++) {
            free(dfusefile->images[i]->imgelement[j]->data);
            free(dfusefile->images[i]->imgelement[j]);
        }
        free(dfusefile->images[i]->tarprefix);
        free(dfusefile->images[i]->imgelement);
        free(dfusefile->images[i]);
    }

    free(dfusefile->images);
    free(dfusefile->suffix);
    free(dfusefile->prefix);
    free(dfusefile);
}