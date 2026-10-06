#include "PluginEditor.h"
#include <iostream>
#include <cmath>
#include <stdexcept>

// Render the real editor without installing the AU or launching Logic.
static void require(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
static void parameter(CSynthProcessor& processor,const char* id,float value)
{
    auto* p=processor.state.getParameter(id); require(p!=nullptr,"Missing parameter");
    p->setValueNotifyingHost(p->convertTo0to1(value));
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try {
        require(argc==3,"Usage: CSynthScreenshots output-folder piano-recordings-folder");
        const auto cwd=juce::File::getCurrentWorkingDirectory();
        const auto folder=cwd.getChildFile(argv[1]), media=cwd.getChildFile(argv[2]);
        require(folder.createDirectory().wasOk(),"Cannot create output folder");
        CSynthProcessor processor;
        for (int i=0;i<SYNTH_PRESET_COUNT;++i)
            if (juce::String(synthPresetName(i))=="Flute Concert") { processor.setCurrentProgram(i); break; }
        parameter(processor,"lowpass",6000); parameter(processor,"highpass",60);
        parameter(processor,"echoMix",.2f); parameter(processor,"reverbMix",.15f);
        parameter(processor,"echoTiming",8); // dotted eighth at the 120 BPM fallback
        processor.prepareToPlay(48000,512);
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        require(editor!=nullptr,"Cannot create editor");
        juce::AudioBuffer<float> buffer(2,512); juce::MidiBuffer midi;
        auto advance=[&] {
            for (int i=0;i<24;++i) {
                processor.processBlock(buffer,midi);
                if (i%4==3) { juce::Thread::sleep(35); juce::Timer::callPendingTimersSynchronously(); }
            }
        };
        auto capture=[&](const char* tab,const char* filename) {
            bool found=false;
            for (auto* child : editor->getChildren())
                if (auto* button=dynamic_cast<juce::TextButton*>(child))
                    if (button->getButtonText()==tab) { button->onClick(); found=true; break; }
            require(found,"Missing editor tab"); advance();
            const auto image=editor->createComponentSnapshot(editor->getLocalBounds());
            juce::FileOutputStream output(folder.getChildFile(filename));
            require(output.openedOk(),"Cannot write screenshot"); output.setPosition(0); output.truncate();
            require(juce::PNGImageFormat().writeImageToStream(image,output),"Cannot encode screenshot");
        };
        processor.keyboard.noteOn(1,69,.8f);
        capture("Synth","editor.png"); capture("Output & filters","output.png");
        capture("Effects","effects.png"); capture("Keyboard","keyboard.png");
        processor.releaseAllNotes(); advance();
        std::vector<CSynthProcessor::SampleFile> files;
        for (int octave=3;octave<=5;++octave) {
            auto file=media.getChildFile("Piano.pp.A"+juce::String(octave)+".wav");
            require(file.existsAsFile(),"Expected Piano.pp.A3.wav, A4.wav and A5.wav in the recordings folder");
            files.push_back({file.getFullPathName(),juce::String(220.0*std::pow(2.0,octave-3),2)});
        }
        processor.setSampleFiles(std::move(files));
        for (auto* child : editor->getChildren())
            if (auto* panel=dynamic_cast<SamplePanel*>(child)) {
                panel->refresh();
                for (auto* control : panel->getChildren())
                    if (auto* button=dynamic_cast<juce::TextButton*>(control))
                        if (button->getButtonText()=="Apply files") button->onClick();
            }
        if (processor.sampleBankSize()!=3)
            throw std::runtime_error(processor.sampleLoadError().toStdString());
        processor.keyboard.noteOn(1,69,.8f);
        capture("Samples","samples.png");
        processor.keyboard.noteOff(1,69,0);
        std::cout<<"Captured all five tabs in "<<folder.getFullPathName()<<'\n';
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
