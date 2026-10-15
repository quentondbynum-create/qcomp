#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    const juce::Colour kBg      = juce::Colour (0xff14141a);
    const juce::Colour kPanel   = juce::Colour (0xff1e1e26);
    const juce::Colour kAccent  = juce::Colour (0xff9d4edd); // Qondabeat purple
    const juce::Colour kWave    = juce::Colour (0xffc77dff);
    const juce::Colour kTextDim = juce::Colour (0xff8a8a99);
}

//==============================================================================
Knob::Knob (const juce::String& name, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    slider.setTextValueSuffix (suffix);
    slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, kPanel.brighter (0.15f));
    slider.setColour (juce::Slider::thumbColourId, kWave);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    nameLabel.setText (name, juce::dontSendNotification);
    nameLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    nameLabel.setColour (juce::Label::textColourId, kTextDim);
    nameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (nameLabel);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    nameLabel.setBounds (area.removeFromBottom (16));
    slider.setBounds (area);
}

//==============================================================================
GRMeter::GRMeter (QCompProcessor& p) : processor (p)
{
    startTimerHz (30);
}

GRMeter::~GRMeter()
{
    stopTimer();
}

void GRMeter::timerCallback()
{
    const float target = processor.lastBlockMaxGR.load();

    if (target > displayedGR)
    {
        displayedGR = target;
    }
    else
    {
        // Meter falls at ~24 dB per second.
        displayedGR = juce::jmax (target, displayedGR - 24.0f / 30.0f);
    }

    if (target > peakHoldGR)
        peakHoldGR = target;
    else
        peakHoldGR = juce::jmax (target, peakHoldGR - 8.0f / 30.0f);

    repaint();
}

void GRMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (bounds, 6.0f);

    const float frac     = juce::jlimit (0.0f, 1.0f, displayedGR / kMaxGR);
    const float peakFrac = juce::jlimit (0.0f, 1.0f, peakHoldGR / kMaxGR);

    auto bar = bounds.reduced (4.0f);
    const float fillH = bar.getHeight() * frac;
    if (fillH > 0.5f)
    {
        g.setColour (kWave);
        g.fillRoundedRectangle (bar.getX(), bar.getBottom() - fillH,
                                bar.getWidth(), fillH, 4.0f);
    }

    // Peak-hold line.
    const float peakY = bar.getBottom() - bar.getHeight() * peakFrac;
    g.setColour (kAccent);
    g.fillRect (bar.getX(), peakY - 1.0f, bar.getWidth(), 2.0f);

    // Scale labels.
    g.setColour (kTextDim);
    g.setFont (juce::Font (10.0f));
    g.drawText ("0",   bar.getX(), bar.getY() - 2.0f, 20, 12, juce::Justification::left);
    g.drawText ("-24", bar.getX(), bar.getBottom() - 12.0f, 30, 12, juce::Justification::left);

    // Numeric readout.
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText (juce::String (displayedGR, 1) + " dB",
                bounds.getX(), bounds.getBottom() + 2.0f, bounds.getWidth(), 14,
                juce::Justification::centred);
}

//==============================================================================
QCompEditor::QCompEditor (QCompProcessor& p)
    : AudioProcessorEditor (&p), processor (p), grMeter (p)
{
    setSize (560, 300);

    logoImage = juce::ImageCache::getFromMemory (BinaryData::headroomlogo_png,
                                                 BinaryData::headroomlogo_pngSize);

    titleLabel.setText ("QCOMP  —  punch compressor", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    meterLabel.setText ("GR", juce::dontSendNotification);
    meterLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    meterLabel.setColour (juce::Label::textColourId, kTextDim);
    meterLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (meterLabel);

    addAndMakeVisible (thresholdKnob);
    addAndMakeVisible (ratioKnob);
    addAndMakeVisible (attackKnob);
    addAndMakeVisible (releaseKnob);
    addAndMakeVisible (kneeKnob);
    addAndMakeVisible (punchKnob);
    addAndMakeVisible (makeupKnob);
    addAndMakeVisible (grMeter);

    auto& apvts = processor.apvts;
    thresholdAttachment = std::make_unique<Attachment> (apvts, QCompProcessor::kParamThreshold, thresholdKnob.slider);
    ratioAttachment     = std::make_unique<Attachment> (apvts, QCompProcessor::kParamRatio,     ratioKnob.slider);
    attackAttachment    = std::make_unique<Attachment> (apvts, QCompProcessor::kParamAttack,    attackKnob.slider);
    releaseAttachment   = std::make_unique<Attachment> (apvts, QCompProcessor::kParamRelease,   releaseKnob.slider);
    kneeAttachment      = std::make_unique<Attachment> (apvts, QCompProcessor::kParamKnee,      kneeKnob.slider);
    punchAttachment     = std::make_unique<Attachment> (apvts, QCompProcessor::kParamPunch,     punchKnob.slider);
    makeupAttachment    = std::make_unique<Attachment> (apvts, QCompProcessor::kParamMakeup,    makeupKnob.slider);
}

QCompEditor::~QCompEditor() = default;

void QCompEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Headroom Audio brand mark — small and subtle, top-right corner.
    if (logoImage.isValid())
        g.drawImageWithin (logoImage, getWidth() - 44, 12, 30, 30,
                           juce::RectanglePlacement::centred, false);
}

void QCompEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    titleLabel.setBounds (area.removeFromTop (28));

    auto body = area;
    auto meterArea = body.removeFromRight (76);

    meterLabel.setBounds (meterArea.removeFromTop (18));
    grMeter.setBounds (meterArea);

    body.removeFromRight (8); // gap

    // Seven knobs across the remaining width.
    const int knobW = body.getWidth() / 7;
    thresholdKnob.setBounds (body.removeFromLeft (knobW).reduced (2));
    ratioKnob.setBounds     (body.removeFromLeft (knobW).reduced (2));
    attackKnob.setBounds    (body.removeFromLeft (knobW).reduced (2));
    releaseKnob.setBounds   (body.removeFromLeft (knobW).reduced (2));
    kneeKnob.setBounds      (body.removeFromLeft (knobW).reduced (2));
    punchKnob.setBounds     (body.removeFromLeft (knobW).reduced (2));
    makeupKnob.setBounds    (body.reduced (2));
}
