#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
QCompProcessor::QCompProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

QCompProcessor::~QCompProcessor() = default;

// Skew factor placing midPoint at the centre of the normalised range
// (same maths as Slider::setSkewFactorFromMidPoint).
static float skewForMidPoint (float rangeMin, float rangeMax, float midPoint)
{
    return std::log (0.5f) / std::log ((midPoint - rangeMin) / (rangeMax - rangeMin));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout QCompProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamThreshold, "Threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -12.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamRatio, "Ratio",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f), 4.0f));

    juce::NormalisableRange<float> attackRange (
        0.1f, 100.0f, 0.01f, skewForMidPoint (0.1f, 100.0f, 5.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamAttack, "Attack", attackRange, 10.0f));

    juce::NormalisableRange<float> releaseRange (
        10.0f, 1000.0f, 0.1f, skewForMidPoint (10.0f, 1000.0f, 100.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamRelease, "Release", releaseRange, 120.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamKnee, "Knee",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamPunch, "Punch",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f), 100.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        kParamMakeup, "Makeup",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    return layout;
}

//==============================================================================
void QCompProcessor::prepareToPlay (double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    envelope = 0.0f;
    lastBlockMaxGR.store (0.0f);
    updateDetectorCoeffs();
}

void QCompProcessor::releaseResources()
{
    envelope = 0.0f;
}

bool QCompProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::stereo()
        || in == juce::AudioChannelSet::mono();
}

void QCompProcessor::updateDetectorCoeffs()
{
    const float attackMs  = apvts.getRawParameterValue (kParamAttack)->load();
    const float releaseMs = apvts.getRawParameterValue (kParamRelease)->load();

    // One-pole smoothing: coeff = e^(-1 / (sr * timeInSeconds)).
    attackCoeff  = std::exp (-1.0f / (float (sampleRate) * attackMs  / 1000.0f));
    releaseCoeff = std::exp (-1.0f / (float (sampleRate) * releaseMs / 1000.0f));
}

float QCompProcessor::gainReductionDb (float envDb, float thresholdDb, float ratio, float kneeDb)
{
    const float x = envDb - thresholdDb;
    const float slope = 1.0f - 1.0f / ratio;

    if (kneeDb < 0.01f) // hard knee
        return x > 0.0f ? x * slope : 0.0f;

    if (2.0f * x <= -kneeDb)
        return 0.0f;

    if (2.0f * x >= kneeDb)
        return x * slope;

    // Soft knee: parabolic blend inside the knee region.
    const float xk = x + kneeDb * 0.5f;
    return slope * xk * xk / (2.0f * kneeDb);
}

//==============================================================================
void QCompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    updateDetectorCoeffs();

    const float thresholdDb = apvts.getRawParameterValue (kParamThreshold)->load();
    const float ratio       = juce::jmax (1.0f, apvts.getRawParameterValue (kParamRatio)->load());
    const float kneeDb      = apvts.getRawParameterValue (kParamKnee)->load();
    const float punch       = juce::jlimit (0.0f, 1.0f,
                                           apvts.getRawParameterValue (kParamPunch)->load() / 100.0f);
    const float trimDb      = apvts.getRawParameterValue (kParamMakeup)->load();

    // Automatic makeup: gain a 0 dBFS signal would lose (hard-knee estimate),
    // capped so extreme settings stay sane. Manual trim sits on top.
    const float autoMakeupDb  = juce::jmin (-thresholdDb * (1.0f - 1.0f / ratio), 30.0f);
    const float totalMakeupDb = juce::jmax (0.0f, autoMakeupDb) + trimDb;
    const float makeupLin     = juce::Decibels::decibelsToGain (totalMakeupDb);

    const float dryMix = 1.0f - punch;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float blockMaxGR = 0.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        // Linked stereo peak detection.
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, s)));

        const float coeff = (peak > envelope) ? attackCoeff : releaseCoeff;
        envelope = coeff * envelope + (1.0f - coeff) * peak;

        float gr = 0.0f;
        if (envelope > 0.0f)
        {
            const float envDb = juce::Decibels::gainToDecibels (envelope);
            gr = gainReductionDb (envDb, thresholdDb, ratio, kneeDb);
        }
        blockMaxGR = juce::jmax (blockMaxGR, gr);

        const float wetGain = juce::Decibels::decibelsToGain (-gr) * makeupLin;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = buffer.getSample (ch, s);
            const float wet = juce::jlimit (-1.0f, 1.0f, dry * wetGain);
            buffer.setSample (ch, s, dry * dryMix + wet * punch);
        }
    }

    lastBlockMaxGR.store (blockMaxGR);
}

//==============================================================================
juce::AudioProcessorEditor* QCompProcessor::createEditor()
{
    return new QCompEditor (*this);
}

bool QCompProcessor::hasEditor() const
{
    return true;
}

//==============================================================================
const juce::String QCompProcessor::getName() const               { return JucePlugin_Name; }
bool QCompProcessor::acceptsMidi() const                        { return false; }
bool QCompProcessor::producesMidi() const                       { return false; }
bool QCompProcessor::isMidiEffect() const                       { return false; }
double QCompProcessor::getTailLengthSeconds() const             { return 0.5; }

int QCompProcessor::getNumPrograms()                            { return 1; }
int QCompProcessor::getCurrentProgram()                         { return 0; }
void QCompProcessor::setCurrentProgram (int)                    {}
const juce::String QCompProcessor::getProgramName (int)         { return "Default"; }
void QCompProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void QCompProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos (destData, true);
    apvts.state.writeToStream (mos);
}

void QCompProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid())
        apvts.replaceState (tree);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QCompProcessor();
}
