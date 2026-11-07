#pragma once
// The four independent RESORA tools and the hand-off a tool can ask for. Workspaces never reference
// each other: they emit a ToolRequest and MainWindow (the only place that knows every tool) routes it.
#include <QStringList>

enum class Tool { Analyzer = 0, Analytics = 1, Compare = 2, Duplicates = 3 };

struct ToolRequest
{
    Tool target = Tool::Analyzer;
    QStringList paths;  // images (Compare: left, right; Analyzer: properties) or empty
};
