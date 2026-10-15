#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

//==============================================================================
// A labeled rotary knob: slider on top, name + value below.
class Knob : public juce::Component
{
public:
    Knob (const juce::String& name, const juce::String& suffix);

    juce::Slider slider;

private:
    void resized() override;

    juce::Label nameLabel;
};

//==============================================================================
// Vertical gain-reduction meter. Reads the processor's atomic GR value at
// 30 Hz, with its own release ballistics so the bar falls smoothly.
class GRMeter : public juce::Component,
                private juce::Timer
{
public:
    explicit GRMeter (QCompProcessor& p);
    ~GRMeter() override;

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    QCompProcessor& processor;
    float displayedGR = 0.0f; // dB, with release falloff
    float peakHoldGR  = 0.0f; // dB, slow-decay peak line

    static constexpr float kMaxGR = 24.0f; // meter range in dB
};

//==============================================================================
class QCompEditor : public juce::AudioProcessorEditor
{
public:
    explicit QCompEditor (QCompProcessor&);
    ~QCompEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    QCompProcessor& processor;

    Knob thresholdKnob { "Threshold", " dB" };
    Knob ratioKnob     { "Ratio",     ""    };
    Knob attackKnob    { "Attack",    " ms" };
    Knob releaseKnob   { "Release",   " ms" };
    Knob kneeKnob      { "Knee",      " dB" };
    Knob punchKnob     { "Punch",     " %"  };
    Knob makeupKnob    { "Makeup",    " dB" };

    GRMeter grMeter;

    juce::Label titleLabel;
    juce::Label meterLabel;
    juce::Image logoImage;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> thresholdAttachment;
    std::unique_ptr<Attachment> ratioAttachment;
    std::unique_ptr<Attachment> attackAttachment;
    std::unique_ptr<Attachment> releaseAttachment;
    std::unique_ptr<Attachment> kneeAttachment;
    std::unique_ptr<Attachment> punchAttachment;
    std::unique_ptr<Attachment> makeupAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QCompEditor)
};
