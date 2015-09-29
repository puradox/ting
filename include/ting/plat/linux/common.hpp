#pragma once

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include <alsa/asoundlib.h>
#include <sndfile.h>

namespace ting {

struct DeviceInfo
{
    std::string name = "default";   // Playback device
    unsigned int rate = 44100;      // Stream rate (bits/second)
    unsigned int channels = 2;      // Count of channels
    unsigned long buffer = 4096;    // Ring buffer size in frames
    unsigned long period = 1024;    // Period size in frames
    int resample = 1;               // Enable alsa-lib resampling
//  double frequency = 440;         // Sinusoidal wave frequency in Hz
//  int verbose = 0;                // Verbose flag
//  int period_event = 0;           // Produce poll event after each period
};

class Device
{
public:
    Device()
    {
        int err;

        // Assign playback device
        if ((err = snd_pcm_open(&m_device, m_info.name.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0)
            report_err("Cannot use audio device", err);
        else
            report_msg("Audio device ready to use.");

        // Set default hardware parameters
        if (m_ready)
        {
            set_parameters();

            // Prepare device for use
            if ((err = snd_pcm_prepare(m_device)) < 0)
                report_err("Cannot prepare audio device for use", err);
            else
                report_msg("Audio device has been fully prepared for use.");
        }

        report_msg("Device: " + m_info.name);
        report_msg("Sample rate: " + std::to_string(m_info.rate));
        report_msg("Channels: " + std::to_string(m_info.channels));
        report_msg("Period size: " + std::to_string(m_info.period));
        report_msg("Buffer size: " + std::to_string(m_info.buffer));
    }

    Device(DeviceInfo info)
    : Device()
    {
        m_info = info;
    }

    ~Device()
    {
        int err;

        if ((err = snd_pcm_close(m_device)) < 0)
            report_err("Cannot release audio device", err);
        else
            report_msg("Released audio device.");
    }

    void play(const std::string filename)
    {
        if (!m_ready)
        {
            report_msg("Device is not ready");
            return;
        }

        SF_INFO info;
        SNDFILE* file = sf_open(filename.c_str(), SFM_READ, &info);

        if (!file)
        {
            report_msg("Unable to open audio file " + filename);
            return;
        }

        report_msg("File: " + filename);
        report_msg("Frames: " + std::to_string(info.frames));
        report_msg("Sample rate: " + std::to_string(info.samplerate));
        report_msg("Channels: " + std::to_string(info.channels));
        report_msg("Format: " + std::to_string(info.format));

        short buffer[m_info.period * info.channels * sizeof(short)];

        int count = 0;
        while ((count = sf_readf_short(file, buffer, m_info.period)) > 0)
        {
            int err = snd_pcm_writei(m_device, buffer, count);
            if (err == -EAGAIN)
                continue;
            else if (err == -EPIPE)
            {
                if ((err = snd_pcm_prepare(m_device)) < 0)
                {
                    report_err("Unable to recover from underrun", err);
                    break;
                }
            }
            else if (err == -ESTRPIPE)
            {
                while ((err = snd_pcm_resume(m_device)) == -EAGAIN)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (err < 0)
                {
                    if ((err = snd_pcm_prepare(m_device)) < 0)
                    {
                        report_err("Cannot recover from suspend", err);
                        break;
                    }
                }
            }
            else if (err < 0)
            {
                report_err("Unable to playback audio file", err);
            }
            else if (err != count)
                report_msg("PCM write differs from PCM read");
        }
    }

    int playback_callback(SNDFILE* file, snd_pcm_sframes_t frames)
    {
        report_msg("Playback called with " + std::to_string(frames) + " frames");

        int count = sf_readf_short(file, m_buffer, frames / 2);
        report_msg("Counted " + std::to_string(count) + " frames");

        int err = snd_pcm_writei(m_device, m_buffer, count * 2);
        report_msg("Wrote " + std::to_string(err) + " frames");

        if (err < 0)
            report_err("Playback failed", err);

        return err;
    }

    void playback(const std::string& filename)
    {
        if (!m_ready)
        {
            report_msg("Device is not ready");
            return;
        }

        // Configure ALSA for playbacks
        int err;
        snd_pcm_sw_params_t* params;

        // Allocate the hardware parameter structure
        if ((err = snd_pcm_sw_params_malloc(&params)) < 0)
            report_err("Cannot allocate software parameter structure", err);

        // Choose all parameters
        if ((err = snd_pcm_sw_params_current(m_device, params)) < 0)
            report_err("Cannot fill software parameter structure", err);

        // Set amount of frames of playback data till wake
        if ((err = snd_pcm_sw_params_set_avail_min(m_device, params, m_info.buffer)) < 0)
            report_err("Cannot set minumum available frame count", err);

        // We'll start the device ourselves
        if ((err = snd_pcm_sw_params_set_start_threshold(m_device, params, 0U)) < 0)
            report_err("Cannot set start mode", err);

        // Apply the software parameters that we've set
        if ((err = snd_pcm_sw_params(m_device, params)) < 0)
            report_err("Cannot set software parameters", err);
        else
            report_msg("Audio device software parameters have been set successfully.");

        // Prepare device for use
        if ((err = snd_pcm_prepare(m_device)) < 0)
            report_err("Cannot prepare audio device for use", err);
        else
            report_msg("Audio device has been fully prepared for use.");

        SF_INFO info;
        SNDFILE* file = sf_open(filename.c_str(), SFM_READ, &info);

        if (!file)
        {
            report_msg("Unable to open audio file " + filename);
            return;
        }

        report_msg("File: " + filename);
        report_msg("Frames: " + std::to_string(info.frames));
        report_msg("Sample rate: " + std::to_string(info.samplerate));
        report_msg("Channels: " + std::to_string(info.channels));
        report_msg("Format: " + std::to_string(info.format));

        int frames;

        while (true)
        {
            // Wait until device is ready for data, or 1s has elapsed
            if ((err = snd_pcm_wait(m_device, 1000)) < 0)
            {
                report_err("Poll failed", err);
                break;
            }

            // Find how much space is available
            if ((frames = snd_pcm_avail_update(m_device)) == -EAGAIN)
                continue;
            else if (frames == -EPIPE)
            {
                if ((err = snd_pcm_prepare(m_device)) < 0)
                {
                    report_err("Unable to recover from underrun", err);
                    break;
                }
            }
            else if (frames == -ESTRPIPE)
            {
                while ((err = snd_pcm_resume(m_device)) == -EAGAIN)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (err < 0)
                {
                    if ((err = snd_pcm_prepare(m_device)) < 0)
                    {
                        report_err("Cannot recover from suspend", err);
                        break;
                    }
                }
            }
            else if (frames < 0)
            {
                report_err("Unable to playback audio file", err);
            }

            frames = frames > 4096 ? 4096 : frames;
            frames /= 2;
            frames *= 2;

            // Deliver the data
            if ((err = playback_callback(file, frames)) != frames)
            {
                report_err("Playback callback failed", err);
                break;
            }
        }
    }

private:
    void report_err(const std::string message, int err)
    {
        std::cerr << message << " (" << snd_strerror(err) << ")" << std::endl;
        m_ready = false;
    }

    void report_msg(const std::string message)
    {
        std::cout << message << std::endl;
    }

    void set_parameters()
    {
        int err;
        snd_pcm_hw_params_t* params;

        // Allocate the hardware parameter structure
        if ((err = snd_pcm_hw_params_malloc(&params)) < 0)
            report_err("Cannot allocate hardware parameter structure", err);

        // Choose all parameters
        if ((err = snd_pcm_hw_params_any(m_device, params)) < 0)
            report_err("Cannot fill hardware parameter structure", err);

        // Enable resampling
        if ((err = snd_pcm_hw_params_set_rate_resample(m_device, params, m_info.resample)) < 0)
            report_err("Resampling setup failed for playback", err);

        // Access Format: interleaved read/write
        if ((err = snd_pcm_hw_params_set_access(m_device, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
            report_err("Cannot set access type", err);

        // Sample Format: signed 16-bit little-endian format
        if ((err = snd_pcm_hw_params_set_format(m_device, params, SND_PCM_FORMAT_S16_LE)) < 0)
            report_err("Cannot set sample format", err);

        // Channels
        if ((err = snd_pcm_hw_params_set_channels(m_device, params, m_info.channels)) < 0)
            report_err("Cannot set channel count", err);

        // Stream rate
        unsigned int rate = m_info.rate;
        if ((err = snd_pcm_hw_params_set_rate_near(m_device, params, &rate, 0)) < 0)
            report_err("Cannot set sample rate to " + std::to_string(m_info.rate), err);
        if (rate < m_info.rate)
        {
            report_msg("Sample rate does not match requested rate. (" +
                       std::to_string(m_info.rate) + " requested, " +
                       std::to_string(rate) + " acquired)");
            m_info.rate = rate;
        }

        // Apply the hardware parameters that we've set
        if ((err = snd_pcm_hw_params(m_device, params)) < 0)
            report_err("Cannot set hardware parameters", err);
        else
            report_msg("Audio device harware parameters have been set successfully.");

        // Buffer Size
        if ((err = snd_pcm_hw_params_get_buffer_size(params, &m_info.buffer)) < 0)
            report_err("Cannot get buffer size", err);

        // Period Size
        int direction;
        if ((err = snd_pcm_hw_params_get_period_size(params, &m_info.period, &direction)) < 0)
            report_err("Cannot get period size", err);

        // Free params now that we're done
        snd_pcm_hw_params_free(params);
    }

private:
    DeviceInfo m_info;
    snd_pcm_t* m_device;
    short m_buffer[4096];
    bool m_ready = true;
};

} // namespace ting
