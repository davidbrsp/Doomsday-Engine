/** @file channel.h  Logical sound playback channel.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef CLIENT_AUDIO_SOUND_H
#define CLIENT_AUDIO_SOUND_H

#include "api_audiod_sfx.h"  // sfxbuffer_t
#include "audio/system.h"
#include "world/p_object.h"  // mobj_t
#include <de/Error>
#include <de/Vector>

// Sound flags.
#define SFXCF_NO_ORIGIN         ( 0x1 )  ///< Sound is coming from a mystical emitter.
#define SFXCF_NO_ATTENUATION    ( 0x2 )  ///< Sound is very, very loud.
#define SFXCF_NO_UPDATE         ( 0x4 )  ///< Channel update is skipped.

namespace audio {

/**
 * Models a playable sound.
 */
class Sound
{
public:
    /// No data buffer is assigned. @ingroup errors
    DENG2_ERROR(MissingBufferError);

public:
    Sound(ISoundPlayer &player);
    virtual ~Sound();

    /**
     * Determines whether a data buffer is assigned.
     */
    bool hasBuffer() const;

    /**
     * Returns the assigned data buffer.
     */
    sfxbuffer_t const &buffer() const;
    void setBuffer(sfxbuffer_t *newBuffer);

    void releaseBuffer();

    int flags() const;
    void setFlags(int newFlags);

    /**
     * Returns the attributed emitter if any (may be @c nullptr).
     */
    struct mobj_s *emitter() const;
    void setEmitter(struct mobj_s *newEmitter);

    void setFixedOrigin(de::Vector3d const &newOrigin);

    /**
     * Calculate priority points for the currently playing. These points are used by
     * the channel mapper to determine which sounds can be overridden by new sounds.
     * Zero is the lowest priority.
     */
    de::dfloat priority() const;

    /**
     * Change the playback frequency modifier to @a newFrequency (Hz).
     */
    Sound &setFrequency(de::dfloat newFrequency);

    /**
     * Change the playback volume modifier to @a newVolume.
     */
    Sound &setVolume(de::dfloat newVolume);

    /**
     * Returns @c true if the sound is currently playing.
     */
    bool isPlaying() const;

    /**
     * Returns the current playback frequency modifier: 1.0 is normal.
     */
    de::dfloat frequency() const;

    /**
     * Returns the current playback volume modifier: 1.0 is max.
     */
    de::dfloat volume() const;

public:  // Playback interface: ----------------------------------------------------

    /**
     * Prepare the buffer for playing a sample by filling the buffer with as much
     * sample data as fits. The pointer to sample is saved, so the caller mustn't
     * free it while the sample is loaded.
     *
     * @note The sample is not reloaded if the buffer is already loaded with data
     * with the same sound ID.
     */
    void load(sfxsample_t &sample);

    /**
     * Stop the sound if playing and forget about any sample loaded in the buffer.
     *
     * @note Just stopping doesn't affect refresh!
     */
    void stop();

    /**
     * Stop the sound if playing and forget about any sample loaded in the buffer.
     *
     * @todo Logically distinct from @ref stop() ? -ds
     */
    void reset();

    /**
     * Start playing the sound loaded in the buffer.
     */
    void play();
    void setPlayingMode(de::dint sfFlags);

    /**
     * Returns the time in tics that the sound was last played.
     */
    de::dint startTime() const;

public:
    /**
     * Called periodically by the audio system's refresh thread, so that the buffer
     * can be filled with sample data, for streaming purposes.
     *
     * @note Don't do anything too time-consuming...
     */
    void refresh();

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_SOUND_H