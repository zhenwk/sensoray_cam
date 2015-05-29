//#define __STDC_CONSTANT_MACROS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>             /* getopt_long() */
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <asm/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>

#include <ros/ros.h>
#include "sensoray_cam/sensoray_cam.h"

#define CLEAR(x) memset (&(x), 0, sizeof (x))

static volatile int g_stopping = 0;

void errno_exit(const char *s)
{
    fprintf (stderr, "%s error %d, %s\n", s, errno, strerror (errno));
    exit(EXIT_FAILURE);
}

int xioctl(int fd, int request, void *arg)
{
    int r;
    do {
        r = ioctl (fd, request, arg);
    }
    while (-1 == r && EINTR == errno);
    return r;
}

//void SensorayCam::sigHandler(int type)
//{
//    g_stopping = 1;
//    printf("stopping\n");

//}

SensorayCam::SensorayCam(){
    dev_name    = NULL;
    io          = IO_METHOD_MMAP;
    fd          = -1;
    n_buffers   = 0;
//    g_stopping  = 0;
    G_quality   = 50;

    bPAL        = 0;
    bSize       = 4; // 4 CIFS(other settings 1 or 2)
}

SensorayCam::~SensorayCam(){

}

void SensorayCam::process_image(const void *p)
{
    fputc ('.', stdout);
    fflush (stdout);
}

int SensorayCam::read_frame (void)
{
    struct v4l2_buffer buf;
unsigned int i;

switch (io) {
case IO_METHOD_READ:
        if (-1 == read (fd, buffers[0].start, buffers[0].length)) {
                switch (errno) {
                case EAGAIN:
                        return 0;

        case EIO:
            /* Could ignore EIO, see spec. */

            /* fall through */

        default:
            errno_exit ("read");
        }
    }

        process_image (buffers[0].start);

    break;

case IO_METHOD_MMAP:
    CLEAR (buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                case EAGAIN:
                        return 0;

        case EIO:
            /* Could ignore EIO, see spec. */

            /* fall through */

        default:
            errno_exit ("VIDIOC_DQBUF");
        }
    }

            assert (buf.index < n_buffers);
    {
        FILE *f1;
        static int cnt = 0;
        char name[100];
        sprintf(name, "test%d.jpg", cnt);
        cnt++;
        f1 = fopen(name, "wb+");
        printf("wrote %d.  press Ctrl-C to stop\n", buf.bytesused);
        fwrite(buffers[buf.index].start, 1, buf.bytesused,f1);//buffers[buf.index].length, f1);
        fclose(f1);
    }

        process_image (buffers[buf.index].start);

    if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
        errno_exit ("VIDIOC_QBUF");

    break;

case IO_METHOD_USERPTR:
    CLEAR (buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_USERPTR;

    if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
        case EAGAIN:
            return 0;

        case EIO:
            /* Could ignore EIO, see spec. */

            /* fall through */

        default:
            errno_exit ("VIDIOC_DQBUF");
        }
    }

    for (i = 0; i < n_buffers; ++i)
        if (buf.m.userptr == (unsigned long) buffers[i].start
            && buf.length == buffers[i].length)
            break;

    assert (i < n_buffers);

        process_image ((void *) buf.m.userptr);

    if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
        errno_exit ("VIDIOC_QBUF");

    break;
}

return 1;
}

bool SensorayCam::grab_image(void)
{
    unsigned int count;

        count = 1;

        while (count-- > 0 && !g_stopping) {
                for (;;) {
                        fd_set fds;
                        struct timeval tv;
                        int r;

                        FD_ZERO (&fds);
                        FD_SET (fd, &fds);

                        /* Timeout. */
                        tv.tv_sec = 5;
                        tv.tv_usec = 0;

                        r = select (fd + 1, &fds, NULL, NULL, &tv);

                        if (-1 == r) {
                                if (EINTR == errno)
                                        continue;

                                errno_exit ("select");
                        }

                        if (0 == r) {
                                fprintf (stderr, "select timeout\n");
                                exit (EXIT_FAILURE);
                        }

            if (read_frame ())
                            break;

            /* EAGAIN - continue select loop. */
                }
        }
}

void SensorayCam::stop_capturing(void)
{
    enum v4l2_buf_type type;

switch (io) {
case IO_METHOD_READ:
    /* Nothing to do. */
    break;

case IO_METHOD_MMAP:
case IO_METHOD_USERPTR:
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    printf("sending VIDIOC_STREAMOFF\n");
    if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
        errno_exit ("VIDIOC_STREAMOFF");

    break;
}
}

void SensorayCam::start_capturing(void)
{
    unsigned int i;
    enum v4l2_buf_type type;

switch (io) {
case IO_METHOD_READ:
    /* Nothing to do. */
    break;

case IO_METHOD_MMAP:
    for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

            CLEAR (buf);

            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_MMAP;
            buf.index       = i;

            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                        errno_exit ("VIDIOC_QBUF");
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
        errno_exit ("VIDIOC_STREAMON");

    break;

case IO_METHOD_USERPTR:
    for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

            CLEAR (buf);

            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_USERPTR;
        buf.index       = i;
        buf.m.userptr	= (unsigned long) buffers[i].start;
        buf.length      = buffers[i].length;

        if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                        errno_exit ("VIDIOC_QBUF");
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
        errno_exit ("VIDIOC_STREAMON");

    break;
}
}

void SensorayCam::uninit_device (void)
{
    unsigned int i;

switch (io) {
case IO_METHOD_READ:
    free (buffers[0].start);
    break;

case IO_METHOD_MMAP:
    for (i = 0; i < n_buffers; ++i)
        if (-1 == munmap (buffers[i].start, buffers[i].length))
            errno_exit ("munmap");
    break;

case IO_METHOD_USERPTR:
    for (i = 0; i < n_buffers; ++i)
        free (buffers[i].start);
    break;
}

free (buffers);
}

void SensorayCam::init_read(unsigned int buffer_size)
{
    buffers = (buffer*)calloc (1, sizeof (*buffers));

    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    buffers[0].length = buffer_size;
    buffers[0].start = malloc (buffer_size);

    if (!buffers[0].start) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }
}
void SensorayCam::init_mmap(void)
{
    struct v4l2_requestbuffers req;
    int rc;
        CLEAR (req);
    printf("mmap\n");
        req.count               = 4;
        req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory              = V4L2_MEMORY_MMAP;
    rc = xioctl (fd, VIDIOC_REQBUFS, &req);
    printf("mmap rc %d\n", rc);

    if (rc == -1) {
                if (EINVAL == errno) {
                        fprintf (stderr, "%s does not support "
                                 "memory mapping\n", dev_name);
                        exit (EXIT_FAILURE);
                } else {
                        errno_exit ("VIDIOC_REQBUFS");
                }
        printf("fail\n");
        }

        if (req.count < 2) {
                fprintf (stderr, "Insufficient buffer memory on %s\n",
                         dev_name);
                exit (EXIT_FAILURE);
        }

        buffers = (buffer*)calloc (req.count, sizeof (*buffers));

        if (!buffers) {
                fprintf (stderr, "Out of memory\n");
                exit (EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
                struct v4l2_buffer buf;

                CLEAR (buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = n_buffers;

                if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
                        errno_exit ("VIDIOC_QUERYBUF");

                buffers[n_buffers].length = buf.length;
                buffers[n_buffers].start =
                        mmap (NULL /* start anywhere */,
                              buf.length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              fd, buf.m.offset);

                if (MAP_FAILED == buffers[n_buffers].start)
                        errno_exit ("mmap");
        }
}

void SensorayCam::init_userp(unsigned int buffer_size)
{
    struct v4l2_requestbuffers req;
    unsigned int page_size;

    page_size = getpagesize ();
    buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

    CLEAR (req);

    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_USERPTR;

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s does not support "
                     "user pointer i/o\n", dev_name);
            exit (EXIT_FAILURE);
        } else {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }

    buffers = (buffer*)calloc (4, sizeof (*buffers));

    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
        buffers[n_buffers].length = buffer_size;
        buffers[n_buffers].start = memalign (/* boundary */ page_size,
                                             buffer_size);

        if (!buffers[n_buffers].start) {
            fprintf (stderr, "Out of memory\n");
            exit (EXIT_FAILURE);
        }
    }
}

void SensorayCam::init_device(void)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
unsigned int min;

    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
            if (EINVAL == errno) {
                    fprintf (stderr, "%s is no V4L2 device\n",
                             dev_name);
                    exit (EXIT_FAILURE);
            } else {
                    errno_exit ("VIDIOC_QUERYCAP");
            }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
            fprintf (stderr, "%s is no video capture device\n",
                     dev_name);
            exit (EXIT_FAILURE);
    }

switch (io) {
case IO_METHOD_READ:
    if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
        fprintf (stderr, "%s does not support read i/o\n",
             dev_name);
        exit (EXIT_FAILURE);
    }

    break;

case IO_METHOD_MMAP:
case IO_METHOD_USERPTR:
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf (stderr, "%s does not support streaming i/o\n",
             dev_name);
        exit (EXIT_FAILURE);
    }

    break;
}

    /* Select video input, video standard and tune here. */

if (bPAL) {
    int type = V4L2_STD_PAL;
    if (-1 == xioctl (fd, VIDIOC_S_STD, &type))
        errno_exit ("VIDIOC_S_FMT");
} else {
    int type = V4L2_STD_NTSC;
    if (-1 == xioctl (fd, VIDIOC_S_STD, &type))
        errno_exit ("VIDIOC_S_FMT");
}
/* set the JPEG compression */
struct v4l2_jpegcompression jc;
jc.quality = G_quality;
if (-1 == xioctl (fd, VIDIOC_S_JPEGCOMP, &jc))
    errno_exit ("VIDIOC_S_JPEGCOMP");
/* verify JPEG quality was set */
if (-1 == xioctl (fd, VIDIOC_S_JPEGCOMP, &jc))
    errno_exit ("VIDIOC_S_JPEGCOMP");
printf("set JPEG compression quality to %d\n", jc.quality);

    CLEAR (fmt);
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;
/* set image for full size */
if (bPAL) {
    /*
     * Note: these values can be queried using a V4L
     * IOCTL call.  For simplicity, we just set them
     * here in this demo with known values.
     */
    switch (bSize) {
    case 4:
    default:
        fmt.fmt.pix.height      = 288*2;
        fmt.fmt.pix.width       = 704;
        break;
    case 2:
        fmt.fmt.pix.height      = 288;
        fmt.fmt.pix.width       = 704;
        break;
    case 1:
        fmt.fmt.pix.height      = 288;
        fmt.fmt.pix.width       = 352;
        break;
    }
} else {
    switch (bSize) {
    case 4:
    default:
        fmt.fmt.pix.height      = 240*2;
        fmt.fmt.pix.width       = 640;
        break;
    case 2:
        fmt.fmt.pix.height      = 240;
        fmt.fmt.pix.width       = 640;
        break;
    case 1:
        fmt.fmt.pix.height      = 240;
        fmt.fmt.pix.width       = 640;
        break;
    }
}

    if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
            errno_exit ("VIDIOC_S_FMT");


    /* Note VIDIOC_S_FMT may change width and height. */
#if 0
/* Buggy driver paranoia. */
min = fmt.fmt.pix.width * 2;
if (fmt.fmt.pix.bytesperline < min)
    fmt.fmt.pix.bytesperline = min;
min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
if (fmt.fmt.pix.sizeimage < min)
    fmt.fmt.pix.sizeimage = min;
#endif
switch (io) {
case IO_METHOD_READ:
    init_read (fmt.fmt.pix.sizeimage);
    break;

case IO_METHOD_MMAP:
    init_mmap ();
    break;

case IO_METHOD_USERPTR:
    init_userp (fmt.fmt.pix.sizeimage);
    break;
}

}

void SensorayCam::close_device(void)
{
    if (-1 == close (fd))
        errno_exit ("close");

    fd = -1;
}

void SensorayCam::open_device(void)
{
    struct stat st;

    if (-1 == stat (dev_name, &st)) {
        fprintf (stderr, "Cannot identify '%s': %d, %s\n",
                 dev_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }

    if (!S_ISCHR (st.st_mode)) {
        fprintf (stderr, "%s is no device\n", dev_name);
        exit (EXIT_FAILURE);
    }

    fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
    ROS_INFO("open_device: fd = %d, dev_name = %s",fd, dev_name);
    if (-1 == fd) {
        fprintf (stderr, "Cannot open '%s': %d, %s\n",
                 dev_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }

}

//void SensorayCam::usage (FILE *fp,int argc, char **argv)
//{
//    fprintf (fp,
//             "Usage: %s [options]\n\n"
//             "Options:\n"
//             "-d | --device name   Video device name. if unspecified, searches for first 2255 channel\n"
//             "-e | --enumerate     Enumerates all the 2255s in the system.\n"
//             "-h | --help          Print this message\n"
//             "-m | --mmap          Use memory mapped buffers\n"
//             "-r | --read          Use read() calls\n"
//             "-u | --userp         Use application allocated buffers\n"
//             "-i | --interp        Interpolate full size fields 4CIFS\n"
//             "-s | --serial        Retrieve serial number(if available)\n"
//             "",
//             argv[0]);
//}

/*int SensorayCam::EnumVideo(int display)
{
    int i, fd, rc;
    int first_2255 = -1;
    char devname[16];
    struct v4l2_capability cap;

    for (i = 0; i < 256; i++) {
        sprintf(devname, "/dev/video%d", i);
        fd = open(devname, O_RDWR);
        if (fd == -1)
            continue;
        if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
            if (strcmp((const char*)cap.card, cardname) == 0) {
                if (first_2255 == -1)
                    first_2255 = i;
                if (display)
                    printf("%s is a 2255 channel\n", devname);
            }
        }
        close(fd);
    }
    return first_2255;
}


int SensorayCam::Check2255(char *devname)
{
    int fd, rc;
    struct v4l2_capability cap;
    rc = -1;
    fd = open(devname, O_RDWR);
    if (fd == -1)
        return -1;
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
        if (strcmp((const char*)cap.card, cardname) == 0)
            rc = 0;
    }
    close(fd);
    return rc;

}*/

//void SensorayCam::test_video_status()
//{
//    struct v4l2_input inp;
//    if (-1 == xioctl (fd, VIDIOC_ENUMINPUT, &inp)) {
//        printf("could not get video input status\n");
//        return;
//    }
//    if (inp.status & V4L2_IN_ST_NO_SIGNAL)
//        printf("no signal connected\n");
//    else
//        printf("video signal connected\n");
//    return;
//}
