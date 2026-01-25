#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
#if !JucePlugin_IsMidiEffect
    #if !JucePlugin_IsSynth
              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
    #endif
              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
    , apvts(*this, nullptr, juce::Identifier("PARAMS"), createParameterLayout())
{
    apvts.addParameterListener(isLinkEnabledParameterID, this);

    // Synchronize the atomic with the parameter's initial value
    if (auto* param = apvts.getRawParameterValue(isLinkEnabledParameterID))
    {
        bool linkEnabled = param->load() > 0.5f;
        isLinkEnabled.store(linkEnabled, std::memory_order_relaxed);
        link.enable(linkEnabled);
    }

    link.setTempoCallback ([this] (double bpm) { ignoreIncomingBpmChange (bpm); });
}

PluginProcessor::~PluginProcessor()
{
    link.enable (false);
    apvts.removeParameterListener(isLinkEnabledParameterID, this);
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
        // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused (sampleRate, samplesPerBlock);
    
    monoBuffer.setSize(1, samplesPerBlock, false, false, true);
    monoBuffer.clear();

    frameBuffer.resize (1024);
    bTrack.updateHopAndFrameSize (512, 1024);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
    #if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    #endif

    return true;
#endif
}

void PluginProcessor::ignoreIncomingBpmChange(double newBPM)
{
    juce::ignoreUnused (newBPM);
    sendTempoToLink(getTempoEstimate());
}

void PluginProcessor::sendTempoToLink(double tempo) {
    if (!isLinkEnabled.load(std::memory_order_relaxed))
        return;
    
    auto timeInMs = link.clock().micros();
    auto sessionState = link.captureAudioSessionState();
    
    sessionState.setTempo (tempo, timeInMs);
    sessionState.setIsPlaying(true, timeInMs);
    
    link.commitAudioSessionState (sessionState);
}

void PluginProcessor::sumInputChannelsToMono (const juce::AudioBuffer<float>& buffer)
{
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();
    monoBuffer.clear();

    if (totalNumInputChannels > 0)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            monoBuffer.addFrom (0, 0, buffer, channel, 0, numSamples, 1.0f / static_cast<float> (totalNumInputChannels));
        }
    }
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    sumInputChannelsToMono (buffer);
    
    auto* channelData = monoBuffer.getReadPointer (0);
    auto numSamples = buffer.getNumSamples();
    for (int i = 0; i < numSamples; ++i)
    {
        frameBuffer[writeIndex++] = static_cast<double> (channelData[i]);

        if (writeIndex >= frameSize)
        {
            bTrack.processAudioFrame (frameBuffer.data());
            if (bTrack.beatDueInCurrentFrame())
            {
                auto tempo = bTrack.getCurrentTempoEstimate();
                setTempoEstimate(tempo);
                // TODO calc REAL beatTime in frame
                //                bTrack.getBeatTimeInSeconds(long frameNumber, <#int hopSize#>, <#int fs#>)
                
                sendTempoToLink(tempo);
            }

            writeIndex = 0; // Reset for next frame
        }
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}


juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    return 
        std::make_unique<juce::AudioParameterBool>(
                                                   juce::ParameterID(isLinkEnabledParameterID, 3),
                                                   "Ableton Link Enabled",
                                                   true
                                                   );
}

void PluginProcessor::parameterChanged(const juce::String& id, float value)
{
    if (id == isLinkEnabledParameterID) {
        bool linkEnabled = value > 0.5f;
        isLinkEnabled.store(linkEnabled, std::memory_order_relaxed);
            link.enable(linkEnabled);
    }
}
