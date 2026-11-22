#pragma once
// The Image Comparison "Insights" section: Quick Verdict on top, then the visual comparisons
// (resolution, file size, dimensions, aspect ratio, technical metadata), then the existing
// written explanations as short collapsible sections. Presentation only; all numbers come from
// Comparison::metrics.
#include <QWidget>

#include "backend/comparison.h"

class AspectIndicator;
class CollapsibleSection;
class DimensionDiagram;
class FileSizeBars;
class MetadataCompare;
class ResolutionChart;

class InsightsView : public QWidget
{
    Q_OBJECT
public:
    explicit InsightsView(const Comparison &c, QWidget *parent = nullptr);

    ResolutionChart *resolution() const { return resolution_; }
    FileSizeBars *fileSize() const { return fileSize_; }
    DimensionDiagram *dimensions() const { return dimensions_; }
    AspectIndicator *aspect() const { return aspect_; }
    MetadataCompare *metadata() const { return metadata_; }
    CollapsibleSection *explanations() const { return explanations_; }
    int verdictCount() const { return verdicts_; }

private:
    ResolutionChart *resolution_ = nullptr;
    FileSizeBars *fileSize_ = nullptr;
    DimensionDiagram *dimensions_ = nullptr;
    AspectIndicator *aspect_ = nullptr;
    MetadataCompare *metadata_ = nullptr;
    CollapsibleSection *explanations_ = nullptr;
    int verdicts_ = 0;
};
