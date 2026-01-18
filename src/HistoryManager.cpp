#include "HistoryManager.h"
#include "ImageProcessor.h"

HistoryManager::HistoryManager(QObject* parent)
    : QObject(parent)
{
}

HistoryManager::~HistoryManager() = default;

void HistoryManager::setImageProcessor(ImageProcessor* processor) {
    m_processor = processor;
}

void HistoryManager::saveState() {
    if (!m_processor || !m_processor->hasImage()) return;

    // Remove any redo states when making a new change
    if (m_currentIndex < static_cast<int>(m_history.size()) - 1) {
        for (size_t i = m_currentIndex + 1; i < m_history.size(); ++i) {
            m_history[i].imageState.release();
        }
        m_history.erase(m_history.begin() + m_currentIndex + 1, m_history.end());
    }

    // Capture current state
    cv::Mat state = m_processor->captureState();

    HistoryState historyState;
    historyState.imageState = state;
    historyState.memorySize = state.total() * state.elemSize();

    m_history.push_back(std::move(historyState));
    m_currentIndex = static_cast<int>(m_history.size()) - 1;

    trimHistory();
    emit historyChanged();
}

void HistoryManager::saveStateBeforeChange() {
    // Only save if we don't already have the current state saved
    // This is called BEFORE making a change
    if (!m_processor || !m_processor->hasImage()) return;
    
    // If we're not at the end of history, we already have states ahead - clear them
    if (m_currentIndex < static_cast<int>(m_history.size()) - 1) {
        for (size_t i = m_currentIndex + 1; i < m_history.size(); ++i) {
            m_history[i].imageState.release();
        }
        m_history.erase(m_history.begin() + m_currentIndex + 1, m_history.end());
    }
    
    emit historyChanged();
}

void HistoryManager::saveInitialState() {
    if (!m_processor || !m_processor->hasImage()) return;
    
    // Clear existing history and save initial state
    m_history.clear();
    m_currentIndex = -1;
    
    cv::Mat state = m_processor->captureState();
    
    HistoryState historyState;
    historyState.imageState = state;
    historyState.memorySize = state.total() * state.elemSize();
    
    m_history.push_back(std::move(historyState));
    m_currentIndex = 0;
    
    emit historyChanged();
}

bool HistoryManager::canUndo() const {
    return m_currentIndex > 0;
}

bool HistoryManager::canRedo() const {
    return m_currentIndex < static_cast<int>(m_history.size()) - 1;
}

void HistoryManager::undo() {
    if (!canUndo() || !m_processor) return;

    m_currentIndex--;
    m_processor->restoreState(m_history[m_currentIndex].imageState);

    emit undoPerformed();
    emit historyChanged();
}

void HistoryManager::redo() {
    if (!canRedo() || !m_processor) return;

    m_currentIndex++;
    m_processor->restoreState(m_history[m_currentIndex].imageState);

    emit redoPerformed();
    emit historyChanged();
}

void HistoryManager::clear() {
    // Clear all history states and free memory
    for (auto& state : m_history) {
        state.imageState.release();
        state.imageState = cv::Mat();
    }
    m_history.clear();
    m_history.shrink_to_fit(); // Actually free the vector memory
    m_currentIndex = -1;
    emit historyChanged();
}

size_t HistoryManager::memoryUsage() const {
    size_t total = 0;
    for (const auto& state : m_history) {
        total += state.memorySize;
    }
    return total;
}

void HistoryManager::trimHistory() {
    // Limit by count
    while (m_history.size() > MAX_HISTORY) {
        m_history.erase(m_history.begin());
        m_currentIndex--;
    }

    // Limit by memory - but ALWAYS keep at least 3 states (initial + 2 undos)
    size_t maxBytes = MAX_MEMORY_MB * 1024 * 1024;
    while (memoryUsage() > maxBytes && m_history.size() > 3) {
        m_history.erase(m_history.begin());
        m_currentIndex--;
    }

    m_currentIndex = std::max(0, m_currentIndex);
}
