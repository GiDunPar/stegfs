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
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <mcrypt.h>
#include <unistd.h>
#include <sys/stat.h>

#include "common/common.h"

#include "src/vstegfs.h"

static uint64_t size_in_mb(char *);

int main(int argc, char **argv)
{
    init("mk" APP, VER);

    uint64_t  fs_size = 0x0;
    char     *fs_name = NULL;

    bool force   = false;
    bool restore = false;

    if (argc < 2)
        return show_usage();
    while (true)
    {
        static struct option long_options[] =
        {
            {"filesystem", required_argument, 0, 'f'},
            {"size"      , required_argument, 0, 's'},
            {"force"     , no_argument      , 0, 'x'},
            {"restore"   , no_argument      , 0, 'r'},
            {"help"      , no_argument      , 0, 'h'},
            {"licence"   , no_argument      , 0, 'l'},
            {"version"   , no_argument      , 0, 'v'},
            {0, 0, 0, 0}
        };
        int32_t optex = 0;
        int32_t opt = getopt_long(argc, argv, "f:s:xrhlv", long_options, &optex);
        if (opt == -1)
            break;
        switch (opt)
        {
            case 'f':
                fs_name = strdup(optarg);
                break;
            case 's':
                fs_size = size_in_mb(optarg);
                break;
            case 'r':
                restore = true;
            case 'x':
                force = true;
                break;
            case 'h':
                return show_help();
            case 'l':
                return show_licence();
            case 'v':
                return show_version();
            case '?':
            default:
                die("unknown option %c", opt);
        }
    }
    /*
     * is this a device or a file?
     */
    int64_t fs = 0x0;
    struct stat fs_stat;
    stat(fs_name, &fs_stat);
    switch (fs_stat.st_mode & S_IFMT)
    {
        case S_IFBLK:
            /*
             * use a device as the file system
             */
            if ((fs = open(fs_name, O_WRONLY | F_WRLCK, S_IRUSR | S_IWUSR)) < 0)
                die("could not open the block device");
            fs_size = lseek(fs, 0, SEEK_END) / SB_1MB;
            break;
        case S_IFDIR:
        case S_IFCHR:
        case S_IFLNK:
        case S_IFSOCK:
        case S_IFIFO:
            die("unable to create file system on specified device");
        case S_IFREG:
            if (!force)
                die("file by that name already exists - use -x to force");
        default:
            /*
             * file doesn't exist - good, lets create it...
             */
            {
                int64_t flags = O_WRONLY | O_CREAT | O_TRUNC | F_WRLCK;
                if (restore)
                    flags ^= O_TRUNC; /* don't truncate the file if we're restoring the sb */
                if ((fs = open(fs_name, flags, S_IRUSR | S_IWUSR)) < 0)
                    die("could not create the file system");
            }
            break;
    }
    uint64_t fs_blocks = 0x0;
    if (restore)
    {
        msg("restoring superblock on %s", fs_name);
        fs_blocks = lseek(fs, 0, SEEK_END) / SB_BLOCK;
    }
    else
    {
        /*
         * check the size of the file system
         */
        if (fs_size < 1)
            die("cannot have a file system with size < 1MB");
        /*
         * display some information about the soon-to-be file system to the
         * user
         */
        msg("location      : %s", fs_name);
        fs_blocks = fs_size * SB_1MB / SB_BLOCK;
        msg("total blocks  : %8lu", fs_blocks);
        {
            char *units = strdup("MB");
            float volume = fs_size;
            if (volume >= SM_1TB)
            {
                volume /= SM_1TB;
                units = strdup("TB");
            }
            else if (volume >= SM_1GB)
            {
                volume /= SM_1GB;
                units = strdup("GB");
            }
            msg("volume        : %8.2f %s", volume, units);
            free(units);
        }
        {
            char *units = strdup("MB");
            float fs_data = fs_size * SB_DATA;
            fs_data /= SB_BLOCK;
            if (fs_data >= SM_1TB)
            {
                fs_data /= SM_1TB;
                units = strdup("TB");
            }
            else if (fs_data >= SM_1GB)
            {
                fs_data /= SM_1GB;
                units = strdup("GB");
            }
            msg("data capacity : %8.2f %s", fs_data, units);
            free(units);
        }
        {
            char *units = strdup("MB");
            float fs_avail = fs_size * SB_DATA;
            fs_avail /= SB_BLOCK;
            fs_avail /= MAX_COPIES;
            if (fs_avail >= SM_1TB)
            {
                fs_avail /= SM_1TB;
                units = strdup("TB");
            }
            else if (fs_avail >= SM_1GB)
            {
                fs_avail /= SM_1GB;
                units = strdup("GB");
            }
            msg("usable space  : %8.2f %s", fs_avail, units);
            free(units);
        }
        /*
         * create lots of noise
         */
        lseek(fs, 0, SEEK_SET);
        for (uint64_t i = 0; i < fs_size; i++)
        {
            vblock_t buffer[BLOCK_PER_MB];

            for (uint16_t j = 0; j < BLOCK_PER_MB; j++)
            {
                uint8_t path[SB_PATH] =
                {
                    0x1B, 0x9D, 0xCC, 0x02,
                    0xA6, 0xAF, 0x57, 0xA1,
                    0xAC, 0xF2, 0x76, 0x8B,
                    0x29, 0x2B, 0xBE, 0x33
                };
                /*
                 * using 8bit bytes makes using rand48() easier
                 */
                uint8_t data[SB_DATA];
                uint8_t hash[SB_HASH];
                uint8_t next[SB_NEXT];

                for (uint8_t k = 0; k < SB_DATA; k++)
                    data[k] = mrand48();
                for (uint8_t k = 0; k < SB_HASH; k++)
                    hash[k] = mrand48();
                for (uint8_t k = 0; k < SB_NEXT; k++)
                    next[k] = mrand48();

                memcpy(buffer[j].path, path, SB_PATH);
                memcpy(buffer[j].data, data, SB_DATA);
                memcpy(buffer[j].hash, hash, SB_HASH);
                memcpy(buffer[j].next, next, SB_NEXT);
            }
            write(fs, buffer, SB_1MB);
        }
    }
    /*
     * write a 'super' block at the beginning
     */
    {
        vblock_t sb;

        char sid[SB_PATH] = { 0x00 };
        strncpy(sid + 1, SUPER_ID, sizeof( sid ) - 2);
        sid[0x00] = 0x02;
        sid[0x0F] = 0x03;
        memcpy(sb.path, sid, SB_PATH);

        memset(sb.data, 0xFF, SB_DATA);

        sb.hash[0] = MAGIC_0;
        sb.hash[1] = MAGIC_1;
        sb.hash[2] = MAGIC_2;

        memcpy(sb.next, &fs_blocks, SB_NEXT);

        lseek(fs, 0, SEEK_SET);
        write(fs, &sb, sizeof( vblock_t ));
    }

    close(fs);
    return errno;
}

uint64_t size_in_mb(char *s)
{
    /*
     * parse the value for our file system size (if using a file on an
     * existing file system) allowing for suffixes: MB, GB or TB - a
     * size less than 1MB is silly :p and more than 1TB is insane
     */
    char *f;
    uint64_t v = strtol(s, &f, 0);
    switch (toupper(f[0]))
    {
        case 'T':
            v *= SM_1TB;
            break;
        case 'G':
            v *= SM_1GB;
            break;
        case 'M':
        case '\0':
            break;
        default:
            die("unknown size suffix %c", f[0]);
    }
    return v;
}

int64_t show_help(void)
{
    show_version();
    show_usage();
    fprintf(stderr, "\nOptions:\n\n");
    fprintf(stderr, "  -f, --filesystem  FILE SYSTEM  Where to write the file system to\n");
    fprintf(stderr, "  -s, --size        SIZE         Size of the file system in MB\n");
    fprintf(stderr, "  -x, --force                    Force overwriting an existing file\n");
    fprintf(stderr, "  -r, --restore                  Restore the default vstegfs superblock\n");
    fprintf(stderr, "  -h, --help                     Show this help list\n");
    fprintf(stderr, "  -l, --licence                  Show overview of GNU GPL\n");
    fprintf(stderr, "  -v, --version                  Show version information\n\n");
    fprintf(stderr, "  A tool for creating vstegfs images.  If a device is used instead of\n");
    fprintf(stderr, "  a file (eg: /dev/sda1) a size is not needed as the file system will\n");
    fprintf(stderr, "  use all available space on that device/partition.\n\n");
    return EXIT_SUCCESS;
}
