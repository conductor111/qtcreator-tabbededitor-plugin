#include "tabbar.h"

#include "constants.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/fileiconprovider.h>
#include <coreplugin/idocument.h>
#include <projectexplorer/session.h>

#include <coreplugin/icore.h>
#include <coreplugin/settingsdatabase.h>

#include <QMenu>
#include <QMouseEvent>
#include <QShortcut>
#include <QTabBar>

#include <vector>
#include <list>

using namespace Core::Internal;

using namespace TabbedEditor::Internal;

/// TODO: Use Core::DocumentModel for everything

TabBar::TabBar(QWidget *parent) :
    QTabBar(parent)
{
    setExpanding(false);
    setMovable(true);
    setTabsClosable(true);
    setUsesScrollButtons(true);

    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sp.setHorizontalStretch(1);
    sp.setVerticalStretch(0);
    sp.setHeightForWidth(sizePolicy().hasHeightForWidth());
    setSizePolicy(sp);

    connect(this, &QTabBar::tabMoved, [this](int from, int to) {
        m_editors.move(from, to);
    });

    Core::EditorManager *em = Core::EditorManager::instance();

    connect(em, &Core::EditorManager::editorOpened, this, &TabBar::addEditorTab);
    connect(em, &Core::EditorManager::editorsClosed, this, &TabBar::removeEditorTabs);
    connect(em, SIGNAL(currentEditorChanged(Core::IEditor*)), SLOT(selectEditorTab(Core::IEditor*)));

    connect(this, &QTabBar::currentChanged, this, &TabBar::activateEditor);
    connect(this, &QTabBar::tabCloseRequested, this, &TabBar::closeTab);

    ProjectExplorer::SessionManager *sm = ProjectExplorer::SessionManager::instance();
    connect(sm, &ProjectExplorer::SessionManager::sessionLoaded, [this, em](QString sessionName) {
        Core::IEditor* curEd = em->currentEditor();
        foreach (Core::DocumentModel::Entry *entry, Core::DocumentModel::entries()) {
            em->activateEditorForEntry(entry, Core::EditorManager::DoNotChangeCurrentEditor);
        }
        if (!Core::DocumentModel::entries().empty() && curEd) {
            em->activateEditor(curEd, Core::EditorManager::DoNotChangeCurrentEditor);
        }
        reorderTabs(sessionName);
        // to scrolling to the active tab after reordering
        setUsesScrollButtons(false);
        setUsesScrollButtons(true);
        //////////////////////////////////////////////////////////////////////////
    });

    connect(sm, &ProjectExplorer::SessionManager::aboutToSaveSession, [this, sm]() {
        saveTabsInfo(sm->activeSession());
    });

    connect(sm, &ProjectExplorer::SessionManager::sessionRenamed, [this, em](const QString &oldName, const QString &newName) {
        if (!Core::ICore::settingsDatabase()->contains("TabsInfo_" + oldName)) {
            return;
        }
        Core::ICore::settingsDatabase()->setValue("TabsInfo_" + newName, Core::ICore::settingsDatabase()->value("TabsInfo_" + oldName));
        Core::ICore::settingsDatabase()->remove("TabsInfo_" + oldName);
    });

    connect(sm, &ProjectExplorer::SessionManager::sessionRemoved, [this, em](const QString &name) {
        Core::ICore::settingsDatabase()->remove("TabsInfo_" + name);
    });

    const QString shortCutSequence = QStringLiteral("Ctrl+Alt+%1");
    for (int i = 1; i <= 10; ++i) {
        QShortcut *shortcut = new QShortcut(shortCutSequence.arg(i % 10), this);
        connect(shortcut, &QShortcut::activated, [this, shortcut]() {
            setCurrentIndex(m_shortcuts.indexOf(shortcut));
        });
        m_shortcuts.append(shortcut);
    }

    QAction *prevTabAction = new QAction(tr("Switch to previous tab"), this);
    Core::Command *prevTabCommand
            = Core::ActionManager::registerAction(prevTabAction,
                                                  TabbedEditor::Constants::PREV_TAB_ID,
                                                  Core::Context(Core::Constants::C_GLOBAL));
    prevTabCommand->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+J")));
    connect(prevTabAction, SIGNAL(triggered()), this, SLOT(prevTabAction()));

    QAction *nextTabAction = new QAction(tr("Switch to next tab"), this);
    Core::Command *nextTabCommand
            = Core::ActionManager::registerAction(nextTabAction,
                                                  TabbedEditor::Constants::NEXT_TAB_ID,
                                                  Core::Context(Core::Constants::C_GLOBAL));
    nextTabCommand->setDefaultKeySequence(QKeySequence(tr("Ctrl+Shift+K")));
    connect(nextTabAction, SIGNAL(triggered()), this, SLOT(nextTabAction()));
}

void TabBar::activateEditor(int index)
{
    if (index < 0 || index >= m_editors.size())
        return;

    Core::EditorManager::instance()->activateEditor(m_editors[index]);
}

void TabBar::addEditorTab(Core::IEditor *editor)
{
    Core::IDocument *document = editor->document();

    const int index = addTab(document->displayName());
    setTabIcon(index, Core::FileIconProvider::icon(document->filePath()));
    setTabToolTip(index, document->filePath().toString());

    m_editors.append(editor);

    connect(document, &Core::IDocument::changed, [this, editor, document]() {
        const int index = m_editors.indexOf(editor);
        if (index == -1)
            return;
        QString tabText = document->displayName();
        if (document->isModified())
            tabText += QLatin1Char('*');
        setTabText(index, tabText);
    });
}

void TabBar::removeEditorTabs(QList<Core::IEditor *> editors)
{
    blockSignals(true); // Avoid calling activateEditor()
    foreach (Core::IEditor *editor, editors) {
        const int index = m_editors.indexOf(editor);
        if (index == -1)
            continue;
        m_editors.removeAt(index);
        removeTab(index);
    }
    blockSignals(false);
}

void TabBar::selectEditorTab(Core::IEditor *editor)
{
    const int index = m_editors.indexOf(editor);
    if (index == -1)
        return;
    setCurrentIndex(index);
}

void TabBar::closeTab(int index)
{
    if (index < 0 || index >= m_editors.size())
        return;

    Core::EditorManager::instance()->closeEditors(QList<Core::IEditor*>{ m_editors.takeAt(index) });
    removeTab(index);
}

void TabBar::prevTabAction()
{
    int index = currentIndex();
    if (index >= 1)
        setCurrentIndex(index - 1);
    else
        setCurrentIndex(count() - 1);
}

void TabBar::nextTabAction()
{
    int index = currentIndex();
    if (index < count() - 1)
        setCurrentIndex(index + 1);
    else
        setCurrentIndex(0);
}

void TabBar::saveTabsInfo(const QString& sessionName)
{
    QByteArray data;
    QDataStream storage(&data, QIODevice::OpenModeFlag::WriteOnly);

    for (int index = 0; index < m_editors.size(); ++index) {
        const Utils::FilePath& filePath = m_editors[index]->document()->filePath();
        if (filePath.isEmpty()) {
            continue;
        }
        storage << filePath.toString();
    }

    Core::ICore::settingsDatabase()->setValue("TabsInfo_" + sessionName, qCompress(data).toBase64());
}

void TabBar::reorderTabs(const QString& sessionName)
{
    QByteArray storedTabsInfoBase64 = Core::ICore::settingsDatabase()->value("TabsInfo_" + sessionName).toByteArray();
    if (storedTabsInfoBase64.isNull() || storedTabsInfoBase64.isEmpty()) {
        return;
    }

    QByteArray ba = QByteArray::fromBase64(storedTabsInfoBase64);
    QByteArray storedTabsInfo = qUncompress(ba);
    if (storedTabsInfo.isNull() || storedTabsInfo.isEmpty()) {
        return;
    }

    struct TStoredItem
    {
        QString filePath;
        int curIndex{ -1 };
        std::list<TStoredItem*>::iterator itCurItemsOrder;
    };
    std::vector<TStoredItem> storedItems;
    std::list<TStoredItem*> curItemsOrder;

    QDataStream storage(&storedTabsInfo, QIODevice::OpenModeFlag::ReadOnly);
    QString filePath;
    for (storage >> filePath; storage.status() == QDataStream::Status::Ok; storage >> filePath) {
        storedItems.push_back({ filePath });
    }

    if (m_editors.size() != storedItems.size()) {
        return;
    }

    for (int index = 0; index < m_editors.size(); ++index) {
        const Utils::FilePath& filePath = m_editors[index]->document()->filePath();

        bool found{ false };
        for (TStoredItem& storedItem : storedItems) {
            if (storedItem.filePath == filePath.toString()) {
                found = true;
                storedItem.curIndex = index;
                curItemsOrder.push_back(&storedItem);
                storedItem.itCurItemsOrder = std::next(curItemsOrder.rbegin()).base();
                break;
            }
        }
        if (!found) {
            return;
        }
    }

    int toIndex = 0;
    for (TStoredItem& storedItem : storedItems) {
        int fromIndex = storedItem.curIndex;

        for (decltype(curItemsOrder)::reverse_iterator it{ storedItem.itCurItemsOrder }; it != curItemsOrder.rend(); ++it) {
            (*it)->curIndex += 1;
        }

        curItemsOrder.erase(storedItem.itCurItemsOrder);
        storedItem.itCurItemsOrder = curItemsOrder.end();

        if (fromIndex != toIndex) {
            moveTab(fromIndex, toIndex);
        }

        ++toIndex;
    }
}

void TabBar::contextMenuEvent(QContextMenuEvent *event)
{
    const int index = tabAt(event->pos());
    if (index == -1)
        return;

    QScopedPointer<QMenu> menu(new QMenu());

    Core::IEditor *editor = m_editors[index];
    Core::DocumentModel::Entry *entry = Core::DocumentModel::entryForDocument(editor->document());
    Core::EditorManager::addSaveAndCloseEditorActions(menu.data(), entry, editor);
    menu->addSeparator();
    Core::EditorManager::addNativeDirAndOpenWithActions(menu.data(), entry);

    menu->exec(mapToGlobal(event->pos()));
}

void TabBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton)
        closeTab(tabAt(event->pos()));
    QTabBar::mouseReleaseEvent(event);
}
