// Usage: hextodfu -o kk.dfu a.hex b.hex

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "crc32.h"
#include "dfuse.h"

int hex2bin(unsigned char *obuf, const char *ibuf, int len)
{
    unsigned char c, c2;

    len = len / 2;
    while (*ibuf != 0) {
        c = *ibuf++;
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'a' && c <= 'f')
            c -= 'a' - 10;
        else if (c >= 'A' && c <= 'F')
            c -= 'A' - 10;
        else
            return -1;

        c2 = *ibuf++;
        if (c2 >= '0' && c2 <= '9')
            c2 -= '0';
        else if (c2 >= 'a' && c2 <= 'f')
            c2 -= 'a' - 10;
        else if (c2 >= 'A' && c2 <= 'F')
            c2 -= 'A' - 10;
        else
            return -1;

        *obuf++ = (c << 4) | c2;
    }
    return len;
}

int check_checksum(uint8_t *inbuf, int len)
{
    unsigned int check = 0;
    while (len--) {
        check += *inbuf++;
    }
    return check & 0xFF;
}

// more details: http://en.wikipedia.org/wiki/Intel_HEX
uint8_t *ihex2bin_buf(unsigned int *start_address, int *dst_len,
                      const char *file)
{
    unsigned int lines = 0, total = 0, oneline_len, elar = 0, pos, cnt;
    char oneline[512];
    uint8_t raw[256], *dst=NULL;
    int start_set = 0;

    FILE *fp = fopen(file, "r");
    if (fp) {
        *dst_len = 1024 * 128;
        dst = malloc(*dst_len); // allocate 128kB of memory for bin data buffer
        if (dst == NULL) {
            *dst_len = -2;
            return NULL;
        }

        *start_address = 0;

        while (fgets(oneline, sizeof(oneline), fp) != NULL) {
            if (oneline[0] == ':') {                    // is valid record?
                oneline_len = strlen(oneline) - 2;      // get line length
                hex2bin(raw, oneline + 1, oneline_len); // convert to bin
                if (check_checksum(raw, oneline_len / 2) ==
                    0) { // check cheksum validity
                    if ((raw[0] == 2) && (raw[1] == 0) && (raw[2] == 0) &&
                        (raw[3] == 4)) { //> Extended Linear Address Record
                                         //:020000040803EF
                        elar = (unsigned int)raw[4] << 24 |
                               (unsigned int)raw[5]
                                   << 16; // gen new address offset
                    } else if ((raw[0] == 0) && (raw[1] == 0) &&
                               (raw[2] == 0) &&
                               (raw[3] ==
                                1)) {     //>End Of File record   :00000001FF
                        *dst_len = total; // return total size of bin data &&
                                          // start address
                        return dst;
                    } else if (raw[3] == 0) { //>Data record - process
                        pos = elar +
                              ((unsigned int)raw[1] << 8 |
                               (unsigned int)
                                   raw[2]); // get start address of this chunk
                        if (start_set == 0) {
                            *start_address =
                                pos;       // set it as new start address - only
                                           // possible for first data record
                            start_set = 1; // only once - this is start address
                                           // of thye binary data
                        }
                        pos -= *start_address;
                        cnt = raw[0]; // get chunk size/length
                        if (pos + cnt >
                            *dst_len) { // enlarge buffer if required
                            unsigned char *dst_new = realloc(
                                dst, *dst_len + 8192); // add 8kB of new space
                            if (dst_new == NULL) {
                                *dst_len = -2; // allocation error - exit
                                free(dst);
                                return NULL;
                            } else {
                                *dst_len += 8192;
                                dst = dst_new; // allocation succesed - copy new
                                               // pointer
                            }
                        }
                        memmove(dst + pos, raw + 4, cnt);
                        if (pos + cnt > total) { // set new total variable
                            total =
                                pos +
                                cnt; // tricky way - file can be none linear!
                        }
                    }
                } else {
                    *dst_len = -1; // checksum error - exit
                    return NULL;
                }
            }
            lines++; // not a IntelHex line - comment?
        }
        *dst_len = -3; // fatal error - no valid intel hex file processed
        free(dst);
    }
    return NULL;
}

void print_help(void)
{
    printf("STM32 hextodfu v0.1\n\n");
    printf("Options:\n");
    printf("-c        - place CRC23 under this address (optional)\n");
    printf("-d        - firmware version number (optional, default: 0xFFFF)\n");
    printf("-h        - help\n");
    printf("-o        - output DFU file name (mandatory)\n");
    printf("-p        - USB ProductID (optional, default: 0xDF11)\n");
    printf("-v        - USB VendorID (optional, default: 0x0483)\n\n");
    printf("Example: hex2dfu -o outfile.dfu file1.hex file2.hex ...\n\n");
}

int main(int argc, char *argv[])
{
    unsigned int add_crc32 = 0;
    int vendor_id = 0x0483, product_id = 0xdf11, device_id = 0xffff;
    const char *outfile = NULL;

    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "hv:p:d:o:c:")) != -1) {
        switch (c) {
        case 'p': // PID
            product_id = strtol(optarg, NULL, 16);
            break;
        case 'v': // VID
            vendor_id = strtol(optarg, NULL, 16);
            break;
        case 'd': // device version
            device_id = strtol(optarg, NULL, 16);
            break;
        case 'c': // place crc32 at this address
            add_crc32 = strtol(optarg, NULL, 16);
            break;
        case 'o': // output file name
            outfile = optarg;
            break;
        case 'h':
            print_help();
            break;
        case '?':
            fprintf(stderr, "Parameter(s) parsing  failed!\n");
            return 1;
        default:
            break;
        }
    }

    int dfufile = open(outfile, O_RDWR | O_CREAT | O_TRUNC,
                       S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    if (dfufile == -1) {
        printf("Could not create %s\n", argv[2]);
        return -2;
    }

    dfuse_file *dfusefile = dfuse_init(device_id, vendor_id, product_id);
    dfuse_image *image = dfuse_addimage(dfusefile, outfile, 0);

    printf("Generating Image \e[1;4;32m%s\e[0m:\n\n", outfile);
    for (int i = optind; i < argc; i++) {
        unsigned int start_address = 0;
        int dst_len = 0;
        printf("   Element: \e[1;4;34m%s\e[0m\n", argv[i]);
        const uint8_t *buf = ihex2bin_buf(&start_address, &dst_len, argv[i]);
        printf("   Address: 0x%.8x - 0x%.8x (%d bytes) \n\n", start_address,
               start_address + dst_len, dst_len);
        if (buf) {
            dfuse_image_element *el =
                dfuse_addelement(dfusefile, image, start_address, dst_len);
            memcpy(el->data, buf, dst_len);
        }
        printf("\n");
    }

    dfuse_writeprefix(dfusefile, dfufile);
    dfuse_writeimages(dfusefile, dfufile);
    dfuse_writesuffix(dfusefile, dfufile);

    // printf("Checksum: <%x>\n", dfusefile->suffix->crc);

    dfuse_struct_cleanup(dfusefile);
    close(dfufile);

    return 0;
}