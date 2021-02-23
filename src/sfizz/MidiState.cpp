// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "MidiState.h"
#include "Macros.h"
#include "Debug.h"

sfz::MidiState::MidiState()
{
    reset();
}

void sfz::MidiState::noteOnEvent(int delay, int noteNumber, float velocity) noexcept
{
    ASSERT(noteNumber >= 0 && noteNumber <= 127);
    ASSERT(velocity >= 0 && velocity <= 1.0);

    if (noteNumber >= 0 && noteNumber < 128) {
        lastNoteVelocities[noteNumber] = velocity;
        noteOnTimes[noteNumber] = internalClock + static_cast<unsigned>(delay);
        lastNotePlayed = noteNumber;
        activeNotes++;
    }

}

void sfz::MidiState::noteOffEvent(int delay, int noteNumber, float velocity) noexcept
{
    ASSERT(delay >= 0);
    ASSERT(noteNumber >= 0 && noteNumber <= 127);
    ASSERT(velocity >= 0.0 && velocity <= 1.0);
    UNUSED(velocity);
    if (noteNumber >= 0 && noteNumber < 128) {
        noteOffTimes[noteNumber] = internalClock + static_cast<unsigned>(delay);
        if (activeNotes > 0)
            activeNotes--;
    }

}

void sfz::MidiState::allNotesOff(int delay) noexcept
{
    for (int note = 0; note < 128; note++)
        noteOffEvent(delay, note, 0.0f);
}

void sfz::MidiState::setSampleRate(float sampleRate) noexcept
{
    this->sampleRate = sampleRate;
    internalClock = 0;
    absl::c_fill(noteOnTimes, 0);
    absl::c_fill(noteOffTimes, 0);
}

void sfz::MidiState::advanceTime(int numSamples) noexcept
{
    auto clearEvents = [] (EventVector& events) {
        ASSERT(!events.empty()); // CC event vectors should never be empty
        events.front().value = events.back().value;
        events.front().delay = 0;
        events.resize(1);
    };

    internalClock += numSamples;
    for (auto& ccEvents : cc)
        clearEvents(ccEvents);

    clearEvents(pitchEvents);
    clearEvents(channelAftertouchEvents);
}

void sfz::MidiState::setSamplesPerBlock(int samplesPerBlock) noexcept
{
    auto updateEventBufferSize = [=] (EventVector& events) {
        events.shrink_to_fit();
        events.reserve(samplesPerBlock);
    };
    this->samplesPerBlock = samplesPerBlock;
    for (auto& ccEvents : cc)
        updateEventBufferSize(ccEvents);

    updateEventBufferSize(pitchEvents);
    updateEventBufferSize(channelAftertouchEvents);
}

float sfz::MidiState::getNoteDuration(int noteNumber, int delay) const
{
    ASSERT(noteNumber >= 0 && noteNumber < 128);
    if (noteNumber < 0 || noteNumber >= 128)
        return 0.0f;

    if (noteOnTimes[noteNumber] != 0 && noteOffTimes[noteNumber] != 0 && noteOnTimes[noteNumber] > noteOffTimes[noteNumber])
        return 0.0f;

    const unsigned timeInSamples = internalClock + static_cast<unsigned>(delay) - noteOnTimes[noteNumber];
    return static_cast<float>(timeInSamples) / sampleRate;
}

float sfz::MidiState::getNoteVelocity(int noteNumber) const noexcept
{
    ASSERT(noteNumber >= 0 && noteNumber <= 127);

    return lastNoteVelocities[noteNumber];
}

float sfz::MidiState::getLastVelocity() const noexcept
{
    return lastNoteVelocities[lastNotePlayed];
}

void sfz::MidiState::insertEventInVector(EventVector& events, int delay, float value)
{
    const auto insertionPoint = absl::c_upper_bound(events, delay, MidiEventDelayComparator {});
    if (insertionPoint == events.end() || insertionPoint->delay != delay)
        events.insert(insertionPoint, { delay, value });
    else
        insertionPoint->value = value;
}

void sfz::MidiState::pitchBendEvent(int delay, float pitchBendValue) noexcept
{
    ASSERT(pitchBendValue >= -1.0f && pitchBendValue <= 1.0f);
    insertEventInVector(pitchEvents, delay, pitchBendValue);
}

float sfz::MidiState::getPitchBend() const noexcept
{
    ASSERT(pitchEvents.size() > 0);
    return pitchEvents.back().value;
}

void sfz::MidiState::channelAftertouchEvent(int delay, float aftertouch) noexcept
{
    ASSERT(aftertouch >= -1.0f && aftertouch <= 1.0f);
    insertEventInVector(channelAftertouchEvents, delay, aftertouch);
}

float sfz::MidiState::getChannelAftertouch() const noexcept
{
    ASSERT(channelAftertouchEvents.size() > 0);
    return channelAftertouchEvents.back().value;
}

void sfz::MidiState::ccEvent(int delay, int ccNumber, float ccValue) noexcept
{
    ASSERT(ccValue >= 0.0 && ccValue <= 1.0);
    insertEventInVector(cc[ccNumber], delay, ccValue);
}

float sfz::MidiState::getCCValue(int ccNumber) const noexcept
{
    ASSERT(ccNumber >= 0 && ccNumber < config::numCCs);
    return cc[ccNumber].back().value;
}

void sfz::MidiState::reset() noexcept
{
    for (auto& velocity: lastNoteVelocities)
        velocity = 0;

    auto clearEvents = [] (EventVector& events) {
        events.clear();
        events.push_back({ 0, 0.0f });
    };

    for (auto& ccEvents : cc)
        clearEvents(ccEvents);

    clearEvents(pitchEvents);
    clearEvents(channelAftertouchEvents);

    activeNotes = 0;
    internalClock = 0;
    lastNotePlayed = 0;
    absl::c_fill(noteOnTimes, 0);
    absl::c_fill(noteOffTimes, 0);
}

void sfz::MidiState::resetAllControllers(int delay) noexcept
{
    for (int ccIdx = 0; ccIdx < config::numCCs; ++ccIdx)
        ccEvent(delay, ccIdx, 0.0f);

    pitchBendEvent(delay, 0.0f);
}

const sfz::EventVector& sfz::MidiState::getCCEvents(int ccIdx) const noexcept
{
    if (ccIdx < 0 || ccIdx >= config::numCCs)
        return nullEvent;

    return cc[ccIdx];
}

const sfz::EventVector& sfz::MidiState::getPitchEvents() const noexcept
{
    return pitchEvents;
}

const sfz::EventVector& sfz::MidiState::getChannelAftertouchEvents() const noexcept
{
    return channelAftertouchEvents;
}
