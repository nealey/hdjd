#include <stdio.h>
#include <malloc.h>

#include <alsa/asoundlib.h>
#include <neaacdec.h>
#include <mp4v2/mp4v2.h>

#include "dump.h"


static int
GetAACTrack(MP4FileHandle *infile)
{
    /* find AAC track */
    MP4TrackId numTracks = MP4GetNumberOfTracks(infile, NULL, 0);
    MP4TrackId i;

    for (i = 1; i <= numTracks; i++)
    {
        uint8_t obj_type;
        const char *track_type = MP4GetTrackType(infile, i);

        if (! track_type) continue;

        if (!MP4_IS_AUDIO_TRACK_TYPE(track_type)) continue;

        /* MP4GetTrackAudioType */
        obj_type = MP4GetTrackEsdsObjectTypeId(infile, i);
        if (obj_type == MP4_INVALID_AUDIO_TYPE)
            continue;
     
        if (obj_type == MP4_MPEG4_AUDIO_TYPE) {
            obj_type = MP4GetTrackAudioMpeg4Type(infile, i);

            if (MP4_IS_MPEG4_AAC_AUDIO_TYPE(obj_type))
                return i;
        } else { 
            if (MP4_IS_AAC_AUDIO_TYPE(obj_type))
                return i;
        }
    }

    /* can't decode this */
    return -1;
}

int
play_file(snd_pcm_t *snd, char *fn)
{

    int track;

    MP4FileHandle infile;

    NeAACDecHandle hDecoder;
    NeAACDecConfigurationPtr config;

    unsigned char *buffer;
    uint32_t buffer_size;
    unsigned long samplerate;
    unsigned char channels;

    long sampleId, numSamples;
    void *sample_buffer;

    infile = MP4Read(fn);
    if (! infile) {
        fprintf(stderr, "Unable to open stream\n");
        return 1;
    }

    if ((track = GetAACTrack(infile)) < 0) {
        fprintf(stderr, "GetAACTrack\n");
        return 1;
    }

    hDecoder = NeAACDecOpen();
    config = NeAACDecGetCurrentConfiguration(hDecoder);
    config->outputFormat = FAAD_FMT_16BIT;
    config->downMatrix = 1;
    config->defObjectType = LC;
    NeAACDecSetConfiguration(hDecoder, config);

    buffer = NULL;
    buffer_size = 0;
    MP4GetTrackESConfiguration(infile, track, &buffer, &buffer_size);

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

    if (buffer) {
        free(buffer);
    }

    DUMP_d(MP4GetTrackMaxSampleSize(infile, track));

    numSamples = MP4GetTrackNumberOfSamples(infile, track);

    DUMP_d(numSamples);
    for (sampleId = 1; sampleId <= numSamples; sampleId++) {
        int rc;
        unsigned int sample_count;
        NeAACDecFrameInfo frameInfo;

        buffer = NULL;
        buffer_size = 0;

        rc = MP4ReadSample(infile, track, sampleId, &buffer, &buffer_size, NULL, NULL, NULL, NULL);
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

int
main(int argc, char *argv[])
{
    int i;
    snd_pcm_t *snd;

    if (snd_pcm_open(&snd, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "Opening ALSA\n");
        return 1;
    }

    for (i = 1; i < argc; i += 1) {
        char *fn = argv[i];

        printf("%s\n", fn);

        play_file(snd, fn);
    }

    return 0;
}
    
