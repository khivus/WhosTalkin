#include "controller.h"
#include <QComboBox>
#include <QDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

namespace wt {
void Controller::persistFilters() {
    settings_.people = filter_.persistent();
    settingsFile_.save(settings_);
    if (!settingsFile_.sync()) source_->log("Could not save people filters; check config directory permissions.");
    refreshPeopleDialog();
}
void Controller::setPersonRule(SpeakerKey key, const std::string& uid, const std::string& name, FilterAction action) {
    filter_.set(key, uid, name, action);
    persistFilters(); render();
}
bool Controller::filterPerson(SpeakerKey key, FilterAction action) {
    if (!key.server || !key.client) return false;
    auto info = source_->client(key);
    if (!info.known) {
        source_->log("This client is no longer available; no people filter was changed."); return false;
    }
    if (info.uniqueId.empty()) {
        if (const auto* active = store_.find(key)) info.uniqueId = active->uniqueId;
    }
    setPersonRule(key, info.uniqueId, info.nickname, action);
    return true;
}
void Controller::openPersonMenu(SpeakerKey key, const QPoint& position) {
    const auto* active = store_.find(key);
    if (!active) return;
    const auto person = *active; // Copy identity: talk stops/reorders while the menu is open.
    auto* menu = new QMenu(window_);
    menu->setObjectName("speakerPersonMenu");
    auto* caption = menu->addAction(QString::fromUtf8(person.nickname.c_str()));
    caption->setEnabled(false);
    const auto current = filter_.rule(key, person.uniqueId);
    for (const auto& entry : std::vector<std::pair<QString, FilterAction>>{
             {"Include this person", FilterAction::Include},
             {"Exclude this person", FilterAction::Exclude},
             {"Remove this person's filter", FilterAction::Clear}}) {
        auto* action = menu->addAction(entry.first);
        if (entry.second != FilterAction::Clear) { action->setCheckable(true); action->setChecked(current == entry.second); }
        const auto operation = entry.second;
        connect(action, &QAction::triggered, this, [this, person, operation] {
            if (person.uniqueId.empty()) {
                const auto* now = store_.find(person.key);
                if (!now || now->sequence != person.sequence) return; // Never reuse a stale session-only identity.
            }
            setPersonRule(person.key, person.uniqueId, person.nickname, operation);
        });
    }
    menu->addSeparator();
    connect(menu->addAction("People filters..."), &QAction::triggered, this, [this] { openPeopleFilters(); });
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popup(position); // No nested event loop or blocking callback.
}
void Controller::refreshPeopleDialog() {
    if (!peopleDialog_) return;
    auto* table = peopleDialog_->findChild<QTableWidget*>("peopleFiltersTable");
    auto* mode = peopleDialog_->findChild<QComboBox*>("peopleFilterMode");
    if (!table || !mode) return;
    const QSignalBlocker blocked(mode);
    mode->setCurrentIndex(settings_.includeOnly ? 1 : 0);
    QString selected;
    if (auto* item = table->item(table->currentRow(), 0)) selected = item->data(Qt::UserRole + 3).toString();
    const auto entries = filter_.entries();
    table->clearSelection();
    table->setCurrentCell(-1, -1);
    table->setRowCount(static_cast<int>(entries.size()));
    int row = 0;
    for (const auto& entry : entries) {
        auto* person = new QTableWidgetItem(QString::fromUtf8(entry.rule.nickname.c_str()));
        const auto uid = QString::fromUtf8(entry.uniqueId.c_str());
        const auto token = uid.isEmpty() ? QString("session:%1:%2").arg(entry.key.server).arg(entry.key.client) : uid;
        person->setData(Qt::UserRole, uid);
        person->setData(Qt::UserRole + 1, QVariant::fromValue<qulonglong>(entry.key.server));
        person->setData(Qt::UserRole + 2, entry.key.client);
        person->setData(Qt::UserRole + 3, token);
        person->setToolTip(uid.isEmpty() ? "This connection only; identity is unavailable." : "TeamSpeak identity: " + uid);
        table->setItem(row, 0, person);
        QString label = entry.rule.action == FilterAction::Include ? "Included" : "Excluded";
        if (uid.isEmpty()) label += " (this connection)";
        table->setItem(row, 1, new QTableWidgetItem(label));
        if (token == selected) table->selectRow(row);
        ++row;
    }
}
void Controller::openPeopleFilters() {
    if (peopleDialog_) { peopleDialog_->show(); peopleDialog_->raise(); peopleDialog_->activateWindow(); return; }
    auto* dialog = new QDialog;
    peopleDialog_ = dialog;
    dialog->setObjectName("whostalkinPeopleFilters");
    dialog->setWindowTitle("WhosTalkin — People Filters");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAttribute(Qt::WA_QuitOnClose, false);
    auto* layout = new QVBoxLayout(dialog);
    auto* mode = new QComboBox(dialog);
    mode->setObjectName("peopleFilterMode");
    mode->addItems({"Everyone except excluded people", "Only included people"});
    layout->addWidget(mode);
    auto* hint = new QLabel("Right-click a person in TeamSpeak or the speaker window to set a rule. Included people still appear only while speaking. Changes here apply immediately.", dialog);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto* table = new QTableWidget(0, 2, dialog);
    table->setObjectName("peopleFiltersTable");
    table->setHorizontalHeaderLabels({"Person", "Rule"});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->verticalHeader()->hide(); layout->addWidget(table);
    auto* buttons = new QHBoxLayout;
    auto changeSelected = [this, table](FilterAction action) {
        auto* item = table->item(table->currentRow(), 0);
        if (!item) return;
        const SpeakerKey key{item->data(Qt::UserRole + 1).toULongLong(), static_cast<std::uint16_t>(item->data(Qt::UserRole + 2).toUInt())};
        const auto uid = item->data(Qt::UserRole).toString().toUtf8().toStdString();
        const auto name = item->text().toUtf8().toStdString();
        setPersonRule(key, uid, name, action);
    };
    for (const auto& entry : std::vector<std::pair<QString, FilterAction>>{
             {"Include selected", FilterAction::Include}, {"Exclude selected", FilterAction::Exclude}, {"Remove rule", FilterAction::Clear}}) {
        auto* button = new QPushButton(entry.first, dialog); buttons->addWidget(button);
        connect(button, &QPushButton::clicked, this, [changeSelected, operation = entry.second] { changeSelected(operation); });
    }
    layout->addLayout(buttons);
    auto* bottom = new QHBoxLayout;
    auto* clear = new QPushButton("Clear all filters", dialog); bottom->addWidget(clear);
    connect(clear, &QPushButton::clicked, this, [this] { filter_.clear(); persistFilters(); render(); });
    bottom->addStretch();
    auto* close = new QPushButton("Close", dialog); bottom->addWidget(close);
    connect(close, &QPushButton::clicked, dialog, &QDialog::close);
    layout->addLayout(bottom);
    connect(mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        auto s = settings_; s.includeOnly = index == 1; applySettings(s);
    });
    refreshPeopleDialog(); dialog->resize(510, 330); dialog->show();
}
}
