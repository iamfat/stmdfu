/*
bintodfu.c :
Takes a .bin file containing the memory image for flashing, and wraps it up in
STM's DfuSe file format.

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

#include "crc32.h"
#include "dfuse.h"

int main(int argc, char *argv[])
{
    int binfile = open(argv[1], O_RDONLY);
    if (binfile == -1) {
        printf("Could not open %s\n", argv[1]);
        return -1;
    } else {
        int dfufile = open(argv[2], O_RDWR | O_CREAT | O_TRUNC,
                           S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        if (dfufile == -1) {
            printf("Could not create %s\n", argv[2]);
            return -2;
        } else {
            dfuse_file *dfusefile = dfuse_init(0xffff, 0x0483, 0x5740);

            dfuse_image *image = dfuse_addimage(dfusefile, argv[2], 0);
            dfuse_readbin(dfusefile, image, binfile);

            dfuse_writeprefix(dfusefile, dfufile);
            dfuse_writeimages(dfusefile, dfufile);
            dfuse_writesuffix(dfusefile, dfufile);

            // 	printf("Checksum: <%x>\n", dfusefile->suffix->crc);

            dfuse_struct_cleanup(dfusefile);

            close(dfufile);
        }

        close(binfile);
    }

    return 0;
}