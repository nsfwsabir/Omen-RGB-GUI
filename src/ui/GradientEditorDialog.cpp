#include "GradientEditorDialog.h"
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QColorDialog>
#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>
#include <QGroupBox>
#include "core/SysFs.h"

GradientEditorDialog::GradientEditorDialog(const QString &initialConfig, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Gradient Configuration");
    resize(560, 420);
    bool ok;
    m_groups = AnimationController::parseGradientConfig(initialConfig, &ok);
    if (!ok && initialConfig.trimmed().isEmpty()) m_groups.clear();

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Each group: zones (0-3) → colors (hex). Groups separated by ';' Example: 0,1,2:FF0000,00FF00,0000FF;3:800080,FFA500", this));

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    auto *btnRow = new QHBoxLayout();
    m_addBtn = new QPushButton("Add Group", this);
    m_editBtn = new QPushButton("Edit Group", this);
    m_removeBtn = new QPushButton("Remove Group", this);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    auto *previewBox = new QGroupBox("Serialized (driver format, max 512 chars, 4 groups, 16 colors each)", this);
    auto *pLay = new QVBoxLayout(previewBox);
    m_preview = new QLineEdit(previewBox);
    m_preview->setReadOnly(true);
    pLay->addWidget(m_preview);
    layout->addWidget(previewBox);

    auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(bbox);

    connect(m_addBtn, &QPushButton::clicked, this, &GradientEditorDialog::addGroup);
    connect(m_removeBtn, &QPushButton::clicked, this, &GradientEditorDialog::removeGroup);
    connect(m_editBtn, &QPushButton::clicked, this, &GradientEditorDialog::editGroup);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](auto){ editGroup(); });
    connect(bbox, &QDialogButtonBox::accepted, this, [this](){
        m_result = AnimationController::buildGradientConfig(m_groups);
        if (m_result.size()>512) {
            QMessageBox::warning(this,"Too long","Config exceeds 512 chars");
            return;
        }
        accept();
    });
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    reloadList();
    updatePreview();
}

void GradientEditorDialog::reloadList() {
    m_list->clear();
    for (int i=0;i<m_groups.size();++i) {
        auto &g = m_groups[i];
        QString zones;
        for (int z=0;z<4;++z) if (g.zoneMask & (1<<z)) zones += (zones.isEmpty()?"":",") + QString::number(z);
        QString entry = QString("Group %1 | Zones: [%2] | Colors: %3").arg(i+1).arg(zones).arg(g.colors.join(", "));
        auto *it = new QListWidgetItem(entry, m_list);
        // color preview via background?
        Q_UNUSED(it);
    }
}

void GradientEditorDialog::updatePreview() {
    QString s = AnimationController::buildGradientConfig(m_groups);
    m_preview->setText(s.isEmpty() ? "(empty — no gradient)" : s);
    m_preview->setToolTip(QString("%1 chars / 512").arg(s.size()));
}

GradientGroup GradientEditorDialog::editGroupDialog(const GradientGroup &g, bool *ok) {
    QDialog dlg(this);
    dlg.setWindowTitle(g.zoneMask==0 ? "Add Group" : "Edit Group");
    dlg.resize(480, 360);
    auto *lay = new QVBoxLayout(&dlg);

    lay->addWidget(new QLabel("Zones (check 0-3):", &dlg));
    QCheckBox *cb0 = new QCheckBox("Zone 0", &dlg);
    QCheckBox *cb1 = new QCheckBox("Zone 1", &dlg);
    QCheckBox *cb2 = new QCheckBox("Zone 2", &dlg);
    QCheckBox *cb3 = new QCheckBox("Zone 3", &dlg);
    cb0->setChecked(g.zoneMask & 1);
    cb1->setChecked(g.zoneMask & 2);
    cb2->setChecked(g.zoneMask & 4);
    cb3->setChecked(g.zoneMask & 8);
    auto *zoneRow = new QHBoxLayout();
    zoneRow->addWidget(cb0); zoneRow->addWidget(cb1); zoneRow->addWidget(cb2); zoneRow->addWidget(cb3);
    lay->addLayout(zoneRow);

    lay->addWidget(new QLabel("Colors (hex RRGGBB, max 16):", &dlg));
    QListWidget *colorList = new QListWidget(&dlg);
    for (auto &c : g.colors) {
        auto *it = new QListWidgetItem(c, colorList);
        QColor col("#"+c);
        it->setBackground(col);
        it->setForeground(col.lightness()>128? Qt::black : Qt::white);
    }
    lay->addWidget(colorList, 1);

    auto *cBtnRow = new QHBoxLayout();
    QPushButton *addCol = new QPushButton("Add Color", &dlg);
    QPushButton *editCol = new QPushButton("Edit", &dlg);
    QPushButton *delCol = new QPushButton("Remove", &dlg);
    QPushButton *pickCol = new QPushButton("Pick Color…", &dlg);
    cBtnRow->addWidget(addCol); cBtnRow->addWidget(pickCol); cBtnRow->addWidget(editCol); cBtnRow->addWidget(delCol);
    cBtnRow->addStretch();
    lay->addLayout(cBtnRow);

    auto getColors = [&]() -> QStringList {
        QStringList out;
        for (int i=0;i<colorList->count();++i) out << colorList->item(i)->text();
        return out;
    };

    QObject::connect(addCol, &QPushButton::clicked, [&](){
        if (colorList->count()>=16) { QMessageBox::warning(&dlg,"Limit","Max 16 colors"); return; }
        bool ok2; QString hex = QInputDialog::getText(&dlg,"Add Color","Hex RRGGBB (e.g. FF0000):", QLineEdit::Normal, "FF0000", &ok2);
        if (!ok2 || hex.isEmpty()) return;
        QString h = SysFs::normalizeHex(hex);
        if (!SysFs::isValidHexColor(h)) { QMessageBox::warning(&dlg,"Invalid","Invalid hex"); return; }
        auto *it = new QListWidgetItem(h, colorList);
        QColor col("#"+h); it->setBackground(col); it->setForeground(col.lightness()>128? Qt::black : Qt::white);
    });
    QObject::connect(pickCol, &QPushButton::clicked, [&](){
        if (colorList->count()>=16) { QMessageBox::warning(&dlg,"Limit","Max 16 colors"); return; }
        QColor c = QColorDialog::getColor(Qt::white, &dlg, "Pick Color");
        if (!c.isValid()) return;
        QString h = QString("%1%2%3").arg(c.red(),2,16,QChar('0')).arg(c.green(),2,16,QChar('0')).arg(c.blue(),2,16,QChar('0')).toUpper();
        auto *it = new QListWidgetItem(h, colorList);
        QColor col("#"+h); it->setBackground(col); it->setForeground(col.lightness()>128? Qt::black : Qt::white);
    });
    QObject::connect(editCol, &QPushButton::clicked, [&](){
        auto *it = colorList->currentItem();
        if (!it) return;
        bool ok2; QString hex = QInputDialog::getText(&dlg,"Edit Color","Hex:", QLineEdit::Normal, it->text(), &ok2);
        if (!ok2) return;
        QString h = SysFs::normalizeHex(hex);
        if (!SysFs::isValidHexColor(h)) { QMessageBox::warning(&dlg,"Invalid","Invalid hex"); return; }
        it->setText(h);
        QColor col("#"+h); it->setBackground(col); it->setForeground(col.lightness()>128? Qt::black : Qt::white);
    });
    QObject::connect(delCol, &QPushButton::clicked, [&](){
        int r = colorList->currentRow();
        if (r>=0) delete colorList->takeItem(r);
    });
    QObject::connect(colorList, &QListWidget::itemDoubleClicked, [&](QListWidgetItem *it){
        bool ok2; QString hex = QInputDialog::getText(&dlg,"Edit Color","Hex:", QLineEdit::Normal, it->text(), &ok2);
        if (!ok2) return;
        QString h = SysFs::normalizeHex(hex);
        if (!SysFs::isValidHexColor(h)) { QMessageBox::warning(&dlg,"Invalid","Invalid hex"); return; }
        it->setText(h);
        QColor col("#"+h); it->setBackground(col); it->setForeground(col.lightness()>128? Qt::black : Qt::white);
    });

    auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(bbox);
    QObject::connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec()!=QDialog::Accepted) { if (ok) *ok=false; return {}; }
    int mask = (cb0->isChecked()?1:0) | (cb1->isChecked()?2:0) | (cb2->isChecked()?4:0) | (cb3->isChecked()?8:0);
    if (mask==0) { QMessageBox::warning(this,"Zones","Select at least one zone"); if (ok) *ok=false; return {}; }
    QStringList cols = getColors();
    if (cols.isEmpty()) { QMessageBox::warning(this,"Colors","Add at least one color"); if (ok) *ok=false; return {}; }
    GradientGroup out; out.zoneMask=mask; out.colors=cols;
    if (ok) *ok=true;
    return out;
}

void GradientEditorDialog::addGroup() {
    if (m_groups.size()>=4) { QMessageBox::warning(this,"Limit","Max 4 groups"); return; }
    bool ok;
    GradientGroup g = editGroupDialog(GradientGroup{}, &ok);
    if (!ok) return;
    m_groups.append(g);
    reloadList(); updatePreview();
}
void GradientEditorDialog::removeGroup() {
    int r = m_list->currentRow();
    if (r<0) return;
    m_groups.removeAt(r);
    reloadList(); updatePreview();
}
void GradientEditorDialog::editGroup() {
    int r = m_list->currentRow();
    if (r<0) return;
    bool ok;
    GradientGroup g = editGroupDialog(m_groups[r], &ok);
    if (!ok) return;
    m_groups[r]=g;
    reloadList(); updatePreview();
}
