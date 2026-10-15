#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

//==============================================================================
/*
    QComp — punch compressor.

    Feedforward peak compressor with linked stereo detection, soft knee,
    automatic makeup gain and a parallel "Punch" mix. Reports the max gain
    reduction of each block through lastBlockMaxGR for the UI meter.
*/
class QCompProcessor : public juce::AudioProcessor
{
public:
    QCompProcessor();
    ~QCompProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Max gain reduction (dB, positive = amount of reduction) measured in the
    // most recent process block. Written on the audio thread, read by the UI.
    std::atomic<float> lastBlockMaxGR { 0.0f };

    juce::AudioProcessorValueTreeState apvts;

    static constexpr const char* kParamThreshold = "threshold"; // dB, -60..0
    static constexpr const char* kParamRatio     = "ratio";     // 1..20
    static constexpr const char* kParamAttack    = "attack";    // ms, 0.1..100
    static constexpr const char* kParamRelease   = "release";   // ms, 10..1000
    static constexpr const char* kParamKnee      = "knee";      // dB, 0..24
    static constexpr const char* kParamPunch     = "punch";     // %, 0..100
    static constexpr const char* kParamMakeup    = "makeup";    // dB, -12..+12

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Pull attack/release from APVTS and recompute detector coefficients.
    void updateDetectorCoeffs();

    // Static gain computer: returns gain reduction in dB (>= 0).
    static float gainReductionDb (float envDb, float thresholdDb, float ratio, float kneeDb);

    double sampleRate = 44100.0;

    float envelope     = 0.0f; // linked stereo peak envelope (linear)
    float attackCoeff  = 0.0f;
    float releaseCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QCompProcessor)
};
