/*
    Copyright 2015-2016 Robert Tari <robert@tari.in>
    Copyright 2011-2015 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>

    This file is part of SACD.

    SACD is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SACD is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SACD.  If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
*/

#include "dst_decoder_mt.h"

#define DSD_SILENCE_BYTE 0x69

dst_decoder_t::dst_decoder_t()
{
    m_channel_count = 0;
    m_samplerate = 0;
    m_framerate = 0;
}

dst_decoder_t::~dst_decoder_t()
{
    frame_slot.state = SLOT_TERMINATING;
    frame_slot.D.close();
}

int dst_decoder_t::init(int channel_count, int samplerate, int framerate)
{
    if (frame_slot.D.init(channel_count, (samplerate / 44100) / (framerate / 75)) == 0)
    {
        frame_slot.channel_count = channel_count;
        frame_slot.samplerate = samplerate;
        frame_slot.framerate = framerate;
        frame_slot.dsd_size = (size_t)(samplerate / 8 / framerate * channel_count);
    }
    else
    {
        return -1;
    }
    this->m_channel_count = channel_count;
    this->m_samplerate    = samplerate;
    this->m_framerate     = framerate;
    this->frame_nr      = 0;
    return 0;
}

int dst_decoder_t::decode(uint8_t* dst_data, size_t dst_size, uint8_t** dsd_data, size_t* dsd_size)
{
    // Allocate encoded frame into the slot
    frame_slot.dsd_data = *dsd_data;
    frame_slot.dst_data = dst_data;
    frame_slot.dst_size = dst_size;
    frame_slot.frame_nr = frame_nr;
    frame_slot.state    = SLOT_EMPTY;
    if (dst_size > 0)
    {
        bool bError = false;
        try
        {
            frame_slot.D.decode(frame_slot.dst_data, frame_slot.dst_size * 8, frame_slot.dsd_data);
        }
        catch (...)
        {
            bError = true;
            frame_slot.D.close();
            frame_slot.D.init(frame_slot.channel_count, frame_slot.samplerate / 44100);
        }
        frame_slot.state = bError ? SLOT_READY_WITH_ERROR : SLOT_READY;
    }
    
    switch (frame_slot.state)
    {
        case SLOT_READY:
            *dsd_data = frame_slot.dsd_data;
            *dsd_size = (size_t)(m_samplerate / 8 / m_framerate * m_channel_count);
            break;
        case SLOT_READY_WITH_ERROR:
            *dsd_data = frame_slot.dsd_data;
            *dsd_size = (size_t)(m_samplerate / 8 / m_framerate * m_channel_count);
            memset(*dsd_data, DSD_SILENCE_BYTE, *dsd_size);
            break;
        default:
            *dsd_data = nullptr;
            *dsd_size = 0;
            break;
    }
    frame_nr++;
    return 0;
}
