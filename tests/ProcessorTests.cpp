#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

#define CHECK(c) do { if (!(c)) throw std::runtime_error(#c); } while(0)
static void set(CSynthProcessor& p, const char* id, float value)
{
    auto* parameter=p.state.getParameter(id); CHECK(parameter);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
static double energy(const juce::AudioBuffer<float>& b, int start=0, int count=-1)
{
    if(count<0) count=b.getNumSamples()-start;
    double result=0;
    for(int i=start;i<start+count;++i) { double x=b.getSample(0,i); CHECK(std::isfinite(x)); result+=x*x; }
    return result;
}
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    try {
        CSynthProcessor a,b;
        a.setCurrentProgram(1); b.setCurrentProgram(1);
        set(a,"attack",0); set(a,"release",0);
        a.prepareToPlay(48000,512); b.prepareToPlay(48000,512);
        CHECK(a.engineReady() && b.engineReady());
        juce::AudioBuffer<float> buffer(2,512); juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1,69,static_cast<juce::uint8>(127)),128);
        midi.addEvent(juce::MidiMessage::noteOff(1,69),384);
        a.processBlock(buffer,midi);
        CHECK(energy(buffer,0,128)<=0 && energy(buffer,128,256)>.01 && energy(buffer,384,128)<=0);
        CHECK(midi.isEmpty());
        for(int i=0;i<512;++i) CHECK(std::abs(buffer.getSample(0,i)-buffer.getSample(1,i))<1.e-12f);
        b.processBlock(buffer,midi); CHECK(energy(buffer)<=0); // Independent instances.

        midi.addEvent(juce::MidiMessage::noteOn(1,69,static_cast<juce::uint8>(100)),0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1,64,127),100);
        midi.addEvent(juce::MidiMessage::noteOff(1,69),200);
        a.processBlock(buffer,midi); CHECK(energy(buffer,250)>0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);
        a.processBlock(buffer,midi); CHECK(energy(buffer)<=0);
        midi.addEvent(juce::MidiMessage::noteOn(1,69,static_cast<juce::uint8>(100)),0);
        midi.addEvent(juce::MidiMessage::noteOn(2,69,static_cast<juce::uint8>(80)),0);
        midi.addEvent(juce::MidiMessage::noteOff(1,69),100);
        a.processBlock(buffer,midi); CHECK(energy(buffer,250)>0);
        midi.addEvent(juce::MidiMessage::controllerEvent(2,123,0),0);
        a.processBlock(buffer,midi); CHECK(energy(buffer)<=0);

        set(a,"lowpass",1200); set(a,"highpass",80); set(a,"l3_o5_ratio",3.14f);
        set(a,"echoMix",.2f); set(a,"gain",-6);
        juce::MemoryBlock state; a.getStateInformation(state);
        b.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        CHECK(std::abs(b.readConfig().filters.lowpassHz-1200)<1);
        CHECK(std::abs(b.readConfig().layers[3].fm.operators[5].ratio-3.14)<.001);
        CHECK(std::abs(b.state.getRawParameterValue("gain")->load()+6)<1.e-5f);
        const char bad[]="not a state"; b.setStateInformation(bad,sizeof(bad));
        CHECK(std::abs(b.state.getRawParameterValue("gain")->load()+6)<1.e-5f);
        for(int i=0;i<SYNTH_PRESET_COUNT;++i) {
            b.setCurrentProgram(i); auto config=b.readConfig(); CHECK(synthConfigValid(&config));
            midi.addEvent(juce::MidiMessage::noteOn(1,72,static_cast<juce::uint8>(100)),0);
            b.processBlock(buffer,midi); energy(buffer);
            midi.addEvent(juce::MidiMessage::noteOff(1,72),0); b.processBlock(buffer,midi);
        }
        auto editor=std::unique_ptr<juce::AudioProcessorEditor>(a.createEditor()); CHECK(editor);
        if (auto* path=std::getenv("CSYNTH_EDITOR_CAPTURE")) {
            auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());
            juce::FileOutputStream output{juce::File(path)}; CHECK(output.openedOk());
            CHECK(juce::PNGImageFormat().writeImageToStream(snapshot,output));
        }
        editor.reset(); a.releaseResources(); b.releaseResources();
        std::cout << "MIDI offsets, sustain, channels, automation/state, 64 programs and editor passed.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
