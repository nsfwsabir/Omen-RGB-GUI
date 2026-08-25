#pragma once
#include <QDialog>
#include <QList>
#include "core/AnimationController.h"

class QListWidget;
class QPushButton;
class QLineEdit;

class GradientEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit GradientEditorDialog(const QString &initialConfig, QWidget *parent = nullptr);
    QString gradientConfig() const { return m_result; }

private slots:
    void addGroup();
    void removeGroup();
    void editGroup();
    void updatePreview();

private:
    QList<GradientGroup> m_groups;
    QString m_result;

    QListWidget *m_list;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_editBtn;
    QLineEdit *m_preview;

    void reloadList();
    GradientGroup editGroupDialog(const GradientGroup &g, bool *ok);
};
