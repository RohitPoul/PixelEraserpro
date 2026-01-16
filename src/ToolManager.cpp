#include "ToolManager.h"
#include <algorithm>

ToolManager::ToolManager(QObject* parent)
    : QObject(parent)
{
}

void ToolManager::setCurrentTool(Tool tool) {
    if (m_currentTool != tool) {
        m_currentTool = tool;
        emit toolChanged(tool);
    }
}

void ToolManager::setBrushSize(int size) {
    size = std::clamp(size, MIN_BRUSH_SIZE, MAX_BRUSH_SIZE);
    if (m_brushSize != size) {
        m_brushSize = size;
        emit brushSizeChanged(size);
    }
}

void ToolManager::setTolerance(int tolerance) {
    tolerance = std::clamp(tolerance, MIN_TOLERANCE, MAX_TOLERANCE);
    if (m_tolerance != tolerance) {
        m_tolerance = tolerance;
        emit toleranceChanged(tolerance);
    }
}

void ToolManager::setBrushHardness(float hardness) {
    hardness = std::clamp(hardness, 0.0f, 1.0f);
    if (std::abs(m_brushHardness - hardness) > 0.001f) {
        m_brushHardness = hardness;
        emit brushHardnessChanged(hardness);
    }
}
