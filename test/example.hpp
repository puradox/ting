#include <alsa/asoundlib.h>
#include <stdio.h>
#include <sndfile.h>
#include <iostream>

#define PCM_DEVICE "default"

void play()
{
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    int err, dir, pcmrc;

    char *infilename = "test/never.wav";
    int readcount;

    SF_INFO sfinfo;
    SNDFILE *infile = NULL;

    infile = sf_open(infilename, SFM_READ, &sfinfo);
    fprintf(stderr,"Channels: %d\n", sfinfo.channels);
    fprintf(stderr,"Sample rate: %d\n", sfinfo.samplerate);
    fprintf(stderr,"Sections: %d\n", sfinfo.sections);
    fprintf(stderr,"Format: %d\n", sfinfo.format);

    /* Open the PCM device in playback mode */
    if ((err = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    /* Allocate parameters object and fill it with default values*/
    if ((err = snd_pcm_hw_params_malloc(&params)) < 0)
    {
        exit(EXIT_FAILURE);
    }
    if ((err = snd_pcm_hw_params_any(pcm_handle, params)) < 0)
    {
        exit(EXIT_FAILURE);
    }
    /* Set parameters */
    if ((err = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        exit(EXIT_FAILURE);
    }
    if ((err = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE)) < 0)
    {
        exit(EXIT_FAILURE);
    }
    if ((err = snd_pcm_hw_params_set_channels(pcm_handle, params, sfinfo.channels)) < 0)
    {
        exit(EXIT_FAILURE);
    }
    if ((err = snd_pcm_hw_params_set_rate(pcm_handle, params, sfinfo.samplerate, 0)) < 0)
    {
        exit(EXIT_FAILURE);
    }

    /* Write parameters */
    if ((err = snd_pcm_hw_params(pcm_handle, params)) < 0)
    {
        exit(EXIT_FAILURE);
    }

    /* Allocate buffer to hold single period */
    if ((err = snd_pcm_hw_params_get_period_size(params, &frames, &dir)) < 0)
    {
        exit(EXIT_FAILURE);
    }
    fprintf(stderr,"# frames in a period: %d\n", (int)frames);

    fprintf(stderr,"Starting read/write loop\n");
    short buf[frames * sfinfo.channels * sizeof(short)];
    std::cout << frames * sfinfo.channels * sizeof(short) << std::endl;
    while ((readcount = sf_readf_short(infile, buf, frames))>0) {

        pcmrc = snd_pcm_writei(pcm_handle, buf, readcount);
        if (pcmrc == -EPIPE) {
            fprintf(stderr, "Underrun!\n");
            snd_pcm_prepare(pcm_handle);
        }
        else if (pcmrc < 0) {
            fprintf(stderr, "Error writing to PCM device: %s\n", snd_strerror(pcmrc));
        }
        else if (pcmrc != readcount) {
            fprintf(stderr,"PCM write difffers from PCM read.\n");
        }

    }
    fprintf(stderr,"End read/write loop\n");

    snd_pcm_close(pcm_handle);
    free(buf);
}

