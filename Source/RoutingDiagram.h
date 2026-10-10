#pragma once
#include "Parameters.h"
#include <juce_gui_basics/juce_gui_basics.h>

// A view of the engine's resolved topology, including fixed algorithms.
class RoutingDiagram final : public juce::Component {
public:
    std::function<void(int)> onSelect;
    void update(const FmConfig& config, int selection, bool enabled) {
        fmInit(&graph,48000,&config);
        selected=selection; active=enabled;
        repaint();
    }
    juce::Rectangle<float> nodeBounds(int op) const {
        int depth[8]{}, count[8]{}, rank=0, maximum=0;
        for (int d=0;d<graph.operatorCount;++d) {
            for (int s=0;s<d;++s)
                if (graph.routing[s][d]>0) depth[d]=juce::jmax(depth[d],depth[s]+1);
            maximum=juce::jmax(maximum,depth[d]); ++count[depth[d]];
        }
        for (int i=0;i<op;++i) if (depth[i]==depth[op]) ++rank;
        float x=maximum ? 18.0f+depth[op]*650.0f/maximum : 330.0f;
        float y=count[depth[op]]==1 ? 88.0f : 20.0f+rank*120.0f/(count[depth[op]]-1);
        float height=count[depth[op]]>1 ? juce::jmin(24.0f,120.0f/(count[depth[op]]-1)-2.0f) : 24.0f;
        return {x,y,68,height};
    }
    void mouseDown(const juce::MouseEvent& event) override {
        auto point=event.position;
        point.x*=820.0f/getWidth(); point.y*=190.0f/getHeight();
        for (int i=0;i<graph.operatorCount;++i)
            if (nodeBounds(i).contains(point) && onSelect) { onSelect(i); return; }
    }
    void paint(juce::Graphics& g) override {
        g.addTransform(juce::AffineTransform::scale(getWidth()/820.0f,getHeight()/190.0f));
        g.setColour(juce::Colour(0xff172332)); g.fillRoundedRectangle(0,0,820,190,7);
        const juce::Colour blue(0xff58bfff), gold(0xffedc36a), pink(0xffee83b5);
        auto arrow=[&](juce::Point<float> a,juce::Point<float> b,juce::Colour colour) {
            g.setColour(colour); g.drawArrow({a,b},1.5f,7,6);
        };
        for (int s=0;s<graph.operatorCount;++s) {
            auto source=nodeBounds(s);
            for (int d=s+1;d<graph.operatorCount;++d) if (graph.routing[s][d]>0) {
                auto dest=nodeBounds(d);
                arrow({source.getRight(),source.getCentreY()},{dest.getX(),dest.getCentreY()},
                      blue.withAlpha(s==selected || d==selected ? 1.0f : .4f));
            }
            if (graph.outputLevels[s]>0) {
                g.setColour(gold.withAlpha(.65f));
                g.drawLine(source.getRight(),source.getCentreY(),758,source.getCentreY(),1.5f);
                g.drawLine(758,source.getCentreY(),758,100,1.5f);
            }
            if (graph.operators[s].config.feedback>0) {
                juce::Path loop;
                loop.startNewSubPath(source.getRight(),source.getCentreY()+7);
                loop.cubicTo(source.getRight()+34,source.getBottom()+20,source.getRight()+34,source.getY()-20,source.getRight(),source.getCentreY()-7);
                g.setColour(pink); g.strokePath(loop,juce::PathStrokeType(1.7f));
                arrow({source.getRight()+7,source.getCentreY()-10},{source.getRight(),source.getCentreY()-7},pink);
            }
        }
        arrow({758,100},{770,100},gold);
        g.setColour(gold); g.drawRoundedRectangle(770,88,42,24,4,1.5f);
        g.setFont(juce::FontOptions(12.0f)); g.drawText("MIX",770,88,42,24,juce::Justification::centred);
        for (int i=0;i<graph.operatorCount;++i) {
            auto bounds=nodeBounds(i);
            g.setColour(graph.outputLevels[i]>0 ? juce::Colour(0xff594630) : juce::Colour(0xff264766));
            g.fillRoundedRectangle(bounds,4);
            g.setColour(i==selected ? juce::Colours::white : blue.withAlpha(.65f));
            g.drawRoundedRectangle(bounds,4,i==selected ? 2.5f : 1.0f);
            g.setColour(juce::Colours::white); g.drawText("OP"+juce::String(i+1),bounds,juce::Justification::centred);
        }
        g.setFont(juce::FontOptions(12.0f));
        g.setColour(blue); g.drawText("Modulation",14,169,110,18,juce::Justification::centredLeft);
        g.setColour(gold); g.drawText("Audio output",137,169,110,18,juce::Justification::centredLeft);
        g.setColour(pink); g.drawText("Self-feedback",267,169,110,18,juce::Justification::centredLeft);
        g.setColour(juce::Colours::white);
        g.drawText(active ? "Click an operator to select its controls" : "FM graph inactive: sample source or inactive layer",390,169,420,18,juce::Justification::centredRight);
    }
private:
    FmSynth graph{};
    int selected=0;
    bool active=true;
};
