// SFML-compat shim — <SFML/Audio.hpp> equivalent for Penguin Dash / Android (A1).
// ETR's audio surface (CMusic/CSound wrap these). Implemented in sfml_compat.cpp —
// stubbed first so the engine links; made real on Oboe + stb_vorbis in A3.
#ifndef PD_SFML_COMPAT_AUDIO_HPP
#define PD_SFML_COMPAT_AUDIO_HPP

#include "System.hpp"
#include <string>

namespace sf {

class SoundSource {
public:
    enum Status { Stopped, Paused, Playing };
    virtual ~SoundSource() {}
    void setVolume(float volume) { m_volume = volume; }
    float getVolume() const { return m_volume; }
    void setLoop(bool loop) { m_loop = loop; }
    bool getLoop() const { return m_loop; }
    virtual void play() = 0;
    virtual void stop() = 0;
    virtual Status getStatus() const { return m_status; }
protected:
    SoundSource() : m_volume(100.f), m_loop(false), m_status(Stopped) {}
    float m_volume;
    bool m_loop;
    Status m_status;
};

class SoundBuffer {
public:
    SoundBuffer();
    ~SoundBuffer();
    bool loadFromFile(const std::string& filename);
    void* impl() const { return m_impl; }
private:
    void* m_impl;
};

class Sound : public SoundSource {
public:
    Sound();
    ~Sound() override;
    void setBuffer(const SoundBuffer& buffer);
    void play() override;
    void stop() override;
private:
    const SoundBuffer* m_buffer;
};

class Music : public SoundSource {
public:
    Music();
    ~Music() override;
    bool openFromFile(const std::string& filename);
    void play() override;
    void stop() override;
private:
    void* m_impl;
};

} // namespace sf

#endif
