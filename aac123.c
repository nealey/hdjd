#include <stdio.h>
#include <malloc.h>

#include <alsa/asoundlib.h>
#include <neaacdec.h>
#include <mp4ff.h>

/* Some things I use for debugging */
#ifdef NODUMP
#  define DUMPf(fmt, args...)
#else
#  define DUMPf(fmt, args...) fprintf(stderr, "%s:%s:%d " fmt "\n", __FILE__, __FUNCTION__, __LINE__, ##args)
#endif
#define DUMP() DUMPf("")
#define DUMP_d(v) DUMPf("%s = %d", #v, v)
#define DUMP_x(v) DUMPf("%s = 0x%x", #v, v)
#define DUMP_s(v) DUMPf("%s = %s", #v, v)
#define DUMP_c(v) DUMPf("%s = '%c' (0x%02x)", #v, v, v)
#define DUMP_p(v) DUMPf("%s = %p", #v, v)


uint32_t
read_callback(void *user_data, void *buffer, uint32_t length)
{
    return fread(buffer, 1, length, (FILE*)user_data);
}
        
uint32_t
seek_callback(void *user_data, uint64_t position)
{
    return fseek((FILE*)user_data, position, SEEK_SET);
}

static int
GetAACTrack(mp4ff_t *infile)
{
    /* find AAC track */
    int i, rc;
    int numTracks = mp4ff_total_tracks(infile);

    for (i = 0; i < numTracks; i++)
    {
        unsigned char *buff = NULL;
        int buff_size = 0;
        mp4AudioSpecificConfig mp4ASC;

        mp4ff_get_decoder_config(infile, i, &buff, &buff_size);

        if (buff)
        {
            rc = NeAACDecAudioSpecificConfig(buff, buff_size, &mp4ASC);
            free(buff);

            if (rc < 0)
                continue;
            return i;
        }
    }

    /* can't decode this */
    return -1;
}


int
main(int argc, char *argv[])
{
    snd_pcm_t *snd;

    int track;

    mp4ff_t *infile;
    mp4ff_callback_t mp4cb;

    NeAACDecHandle hDecoder;
    NeAACDecConfigurationPtr config;
    mp4AudioSpecificConfig mp4ASC;

    unsigned char *buffer;
    int buffer_size;
    unsigned long samplerate;
    unsigned char channels;

    long sampleId, numSamples;
    void *sample_buffer;

    if (snd_pcm_open(&snd, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "Opening ALSA\n");
        return 1;
    }
    


    mp4cb.read = read_callback;
    mp4cb.seek = seek_callback;
    mp4cb.user_data = stdin;

    infile = mp4ff_open_read(&mp4cb);
    if (! infile) {
        fprintf(stderr, "Opening stdin fail\n");
        return 1;
    }

    if ((track = GetAACTrack(infile)) < 0) {
        fprintf(stderr, "GetAACTrack\n");
        return 1;
    }

    hDecoder = NeAACDecOpen();
    config = NeAACDecGetCurrentConfiguration(hDecoder);
    config->outputFormat = FAAD_FMT_16BIT;
    NeAACDecSetConfiguration(hDecoder, config);

    buffer = NULL;
    buffer_size = 0;
    mp4ff_get_decoder_config(infile, track, &buffer, &buffer_size);

    if (NeAACDecInit2(hDecoder, buffer, buffer_size, &samplerate, &channels) < 0) {
        fprintf(stderr, "Initializing decoder\n");
        return 1;
    }
    if (snd_pcm_set_params(snd,
                           SND_PCM_FORMAT_S16,
                           SND_PCM_ACCESS_RW_INTERLEAVED,
                           channels,
                           samplerate,
                           1,
                           500000) < 0) {
        fprintf(stderr, "Set ALSA params\n");
        return 1;
    }
    DUMP_d(channels);
    DUMP_d(samplerate);

    if (buffer) {
        free(buffer);
    }

    numSamples = mp4ff_num_samples(infile, track);

    for (sampleId = 0; sampleId < numSamples; sampleId++) {
        int rc;
        long dur;
        unsigned int sample_count;
        unsigned int delay = 0;
        NeAACDecFrameInfo frameInfo;

        buffer = NULL;
        buffer_size = 0;

        dur = mp4ff_get_sample_duration(infile, track, sampleId);
        rc = mp4ff_read_sample(infile, track, sampleId, &buffer, &buffer_size);
        if (rc == 0) {
            fprintf(stderr, "Read failed\n");
            return 1;
        }

        sample_buffer = NeAACDecDecode(hDecoder, &frameInfo, buffer, buffer_size);
        sample_count = frameInfo.samples;

        snd_pcm_writei(snd, sample_buffer, sample_count / channels);
    }

    return 0;
}
