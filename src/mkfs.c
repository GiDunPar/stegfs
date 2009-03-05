/*
 * vstegfs ~ a virtual steganographic file system for linux
 * Copyright (c) 2007-2009, albinoloverats ~ Software Development
 * email: vstegfs@albinoloverats.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/stat.h>

#include "src/vstegfs.h"
#include "src/serpent.h"

#define NAME "mkvstegfs"

uint64_t filesystem;

int main(int argc, char **argv)
{
    char *fs = NULL;
    bool force = false;
    errno = EXIT_SUCCESS;
    
    if (argc < 2)
    {
        show_usage();
        return EXIT_FAILURE;
    }
    while (true)
    {
        static struct option long_options[] = {
            {"filesystem", required_argument, 0, 'f'},
            {"size"      , required_argument, 0, 's'},
            {"force"     , no_argument      , 0, 'x'},
            {"help"      , no_argument      , 0, 'h'},
            {"licence"   , no_argument      , 0, 'l'},
            {"version"   , no_argument      , 0, 'v'},
            {0, 0, 0, 0}
        };
        int32_t optex = 0;
        int32_t opt = getopt_long(argc, argv, "f:s:xhlv", long_options, &optex);
        if (opt == -1)
            break;
        switch (opt)
        {
            case 'f':
                fs = strdup(optarg);
                break;
            case 's':
                filesystem_size = strtoll(optarg, NULL, 0);
                break;
            case 'x':
                force = true;
                break;
            case 'h':
                show_help();
                return EXIT_SUCCESS;
            case 'l':
                show_licence();
                return EXIT_SUCCESS;
            case 'v':
                show_version();
                return EXIT_SUCCESS;
            case '?':
                return EXIT_FAILURE;
            default:
                return EXIT_FAILURE;
        }
    }
    /*
     * is this a device or a file?
     */
	uint64_t fs_size = 0x0;
    struct stat *fs_stat = calloc(1, sizeof( struct stat ));
    stat(fs, fs_stat);
    switch (fs_stat->st_mode & S_IFMT)
    {
        case S_IFBLK:
            /*
             * use a device as the file system
             */
            if ((filesystem = open(fs, O_WRONLY | F_WRLCK, S_IRUSR | S_IWUSR)) < 0)
            {
                perror("Could not create the file system");
                return errno;
            }
            fs_size = lseek(filesystem, 0, SEEK_END) / 1048576;
            break;
        case S_IFREG:
            if (force)
                break;
            fprintf(stderr, "File by that name already exists - use -x to force\n");
            return EEXIST;
        case S_IFDIR:
        case S_IFCHR:
        case S_IFLNK:
        case S_IFSOCK:
        case S_IFIFO:
            fprintf(stderr, "Unable to create file system on specified device\n");
            return ENODEV;
        default:
            /*
             * file doesn't exist - good, lets create it...
             */
            if ((filesystem = open(fs, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC | F_WRLCK, S_IRUSR | S_IWUSR)) < 0)
            {
                perror("Could not create the file system");
                return errno;
            }
            break;
    }
    /*
     * check the size of the file system
     */
    if (fs_size < 1)
    {
        fprintf(stderr, "Cannot have a file system with size < 1MB\n");
        return EXIT_FAILURE;
    }

    fprintf(stdout, "Location : %s\n", fs);
#ifdef __amd64__
    fprintf(stdout, "Size     : %lu MB (%lu blocks)\n", fs_size, fs_size * SB_BLOCK);
    fprintf(stdout, "Usable   : %lu MB (Approx)\n", fs_size * 5 / 8);
#else
    fprintf(stdout, "Size     : %llu MB (%llu blocks)\n", fs_size, fs_size * SB_BLOCKS);
    fprintf(stdout, "Usable   : %llu MB (Approx)\n", fs_size * 5 / 8);
#endif

    uint8_t *data = malloc(DATA);
    srand48(time(0));

    unsigned char *IV = calloc(SERPENT_B, sizeof( char ));
    char key_mat[1];
    uint32_t *subkeys = generate_key(key_mat);

//    uint16_t pc = 0;
    for (uint64_t i = 0; i < filesystem_size * BLOCKS; i++)
    {
        uint64_t next = 0;
        for (uint32_t j = 0; j < DATA; j++)
            data[j] = lrand48() % 0xFF;
        for (uint32_t j = 0; j < NEXT; j++)
            next = (next << BYTE) | (lrand48() % 0xFF);
        key_mat[0] = (lrand48() % 0x08) + 48;
        
        errno = block_write(key_mat, data, next, IV, subkeys);

//        if (pc < (uint16_t)((100 * i) / (filesystem_size * BLOCKS)))
        {
            fprintf(stdout, "\r%3i%% ", (uint16_t)((100 * i) / (filesystem_size * BLOCKS)));
            if (((i * 100) / (filesystem_size * BLOCKS)) >= 100)
                fprintf(stdout, "##########");
            else if (((i * 100) / (filesystem_size * BLOCKS)) >= 90)
                fprintf(stdout, "#########");
            else if (((i * 100) / (filesystem_size * BLOCKS)) >= 80)
                fprintf(stdout, "########");
            else if (((i * 100) / (filesystem_size * BLOCKS)) >= 70)
                fprintf(stdout, "#######");
            else if (((i * 100) / (filesystem_size * BLOCKS)) >= 60)
                fprintf(stdout, "######");
            else if (((i * 100) / (filesystem_size * BLOCKS)) >= 50)
                fprintf(stdout, "#####");
            else if (((i * 100) / (filesystem_size * BLOCKS)) >= 40)
                fprintf(stdout, "####");
            else if (((i * 100) / (filesystem_size * BLOCKS)) >= 30)
                fprintf(stdout, "###");
            else if (((i * 100) / (filesystem_size * BLOCKS)) >= 20)
                fprintf(stdout, "##");
            else if (((i * 100) / (filesystem_size * BLOCKS)) >= 10)
                fprintf(stdout, "#");
//            pc = (100 * i) / (filesystem_size * BLOCKS);
        }
    }

    close(filesystem);
    return errno;
}

void show_help(void)
{
    show_version();
    fprintf(stderr, "\n");
    show_usage();
    fprintf(stderr, "\n  -f, --filesystem  FILE SYSTEM  Where to write the file system to");
    fprintf(stderr, "\n  -s, --size        SIZE         Size of the file system in MB");
    fprintf(stderr, "\n  -x, --force                    Force overwriting an existing file");
    fprintf(stderr, "\n  -h, --help                     Show this help list");
    fprintf(stderr, "\n  -l, --licence                  Show overview of GNU GPL");
    fprintf(stderr, "\n  -v, --version                  Show version information");
    fprintf(stderr, "\n");
    fprintf(stderr, "\n  A tool for creating vstegfs file system images. If a device is used");
    fprintf(stderr, "\n  instead of a file (eg: /dev/sda1) a size is not needed as the file");
    fprintf(stderr, "\n  system will use all available space on that device/partition.");
    fprintf(stderr, "\n");
}

void show_licence(void)
{
    fprintf(stderr, "This program is free software: you can redistribute it and/or modify\n");
    fprintf(stderr, "it under the terms of the GNU General Public License as published by\n");
    fprintf(stderr, "the Free Software Foundation, either version 3 of the License, or\n");
    fprintf(stderr, "(at your option) any later version.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "This program is distributed in the hope that it will be useful,\n");
    fprintf(stderr, "but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
    fprintf(stderr, "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
    fprintf(stderr, "GNU General Public License for more details.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "You should have received a copy of the GNU General Public License\n");
    fprintf(stderr, "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n");
}

void show_usage(void)
{
    fprintf(stderr, "Usage:\n  %s [OPTION] [ARGUMENT]\n", NAME);
}

void show_version(void)
{
    fprintf(stderr, "%s\n", NAME);
}
