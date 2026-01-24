#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <QObject>
#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>

class ImageProcessor;

class HistoryManager : public QObject {
    Q_OBJECT

public:
    explicit HistoryManager(QObject* parent = nullptr);
    ~HistoryManager();

    void setImageProcessor(ImageProcessor* processor);

    // Save current state before an operation
    void saveState();
    
    // Clear redo states before making a change (call before change, then saveState after)
    void saveStateBeforeChange();
    
    // Save initial state when image is loaded
    void saveInitialState();

    // Undo/Redo
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();

    // Clear history
    void clear();

    // Info - actual undo/redo steps available
    int undoSteps() const { return m_currentIndex; }
    int redoSteps() const { return static_cast<int>(m_history.size()) - m_currentIndex - 1; }
    size_t memoryUsage() const;

signals:
    void historyChanged();
    void undoPerformed();
    void redoPerformed();

private:
    struct HistoryState {
        cv::Mat imageState;
        size_t memorySize;
    };

    ImageProcessor* m_processor = nullptr;
    std::vector<HistoryState> m_history;
    int m_currentIndex = -1;

    static constexpr int MAX_HISTORY = 10;
    static constexpr size_t MAX_MEMORY_MB = 2048; // Max 2GB for history (handles large images)

    void trimHistory();
};

#endif // HISTORYMANAGER_H
