#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#if (MSVC)
#include "ipps.h"
#endif

#include "BTrack.h"
#include <ableton/Link.hpp>

class PluginProcessor : public juce::AudioProcessor
    , public juce::AudioProcessorValueTreeState::Listener
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void extracted(double tempo);
    
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
    
    /// BEGIN BTrackAbletonLink specific
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged(const juce::String& id, float value) override;
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    
    void sendTempoToLink(double tempo);
    void sumInputChannelsToMono(const juce::AudioBuffer<float>& buffer);
    void resampleMonoBuffer();
    void ignoreIncomingBpmChange(double newBPM);
    
    double getTempoEstimate() const { return tempoEstimate.load(std::memory_order_acquire); }
   
    juce::String isLinkEnabledParameterID = "isLinkEnabledParameterID";
protected:
    void setTempoEstimate(double newTempoEstimate) { tempoEstimate.store(newTempoEstimate, std::memory_order_release); }

private:
    BTrack bTrack;
    
    juce::AudioBuffer<float> monoBuffer;
    juce::AudioBuffer<float> resampledBuffer;
    double currentSampleRate = 44100.0;
    const double targetSampleRate = 44100.0;

    std::vector<double> frameBuffer; // Use double for BTrack
    int writeIndex = 0;
    const int frameSize = 1024;
    
    std::atomic<double> tempoEstimate {0.0};
    
    ableton::Link link { 120.0 };
    
    juce::AudioProcessorValueTreeState apvts;
    
    std::atomic<bool> isLinkEnabled { true };

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
