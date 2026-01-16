#ifndef TOOLMANAGER_H
#define TOOLMANAGER_H

#include <QObject>

class ToolManager : public QObject {
    Q_OBJECT

public:
    enum Tool {
        AutoColor = 0,
        ManualErase = 1,
        Repair = 2
    };

    explicit ToolManager(QObject* parent = nullptr);

    // Current tool
    Tool currentTool() const { return m_currentTool; }
    void setCurrentTool(Tool tool);

    // Brush settings
    int brushSize() const { return m_brushSize; }
    void setBrushSize(int size);

    // Tolerance for auto-color
    int tolerance() const { return m_tolerance; }
    void setTolerance(int tolerance);

    // Brush hardness (0.0 = soft, 1.0 = hard)
    float brushHardness() const { return m_brushHardness; }
    void setBrushHardness(float hardness);

signals:
    void toolChanged(Tool tool);
    void brushSizeChanged(int size);
    void toleranceChanged(int tolerance);
    void brushHardnessChanged(float hardness);

private:
    Tool m_currentTool = AutoColor;
    int m_brushSize = 10;
    int m_tolerance = 50;
    float m_brushHardness = 0.8f;

    static constexpr int MIN_BRUSH_SIZE = 1;
    static constexpr int MAX_BRUSH_SIZE = 200;
    static constexpr int MIN_TOLERANCE = 0;
    static constexpr int MAX_TOLERANCE = 255;
};

#endif // TOOLMANAGER_H
