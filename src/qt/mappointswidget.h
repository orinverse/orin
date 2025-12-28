// Copyright (c) 2025 The Orin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ORIN_QT_MAPPOINTSWIDGET_H
#define ORIN_QT_MAPPOINTSWIDGET_H

#include <QWidget>

#include <univalue.h>

#include <memory>

class AddressTableModel;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class WalletModel;

class MapPointsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapPointsWidget(QWidget* parent = nullptr);

    void setWalletModel(WalletModel* model);

private Q_SLOTS:
    void handleCreatePoint();
    void handleTransferPoint();
    void refreshPoints();
    void updateAddressList();
    void updateButtons();

private:
    WalletModel* m_wallet_model{nullptr};
    AddressTableModel* m_address_model{nullptr};
    QComboBox* m_address_combo{nullptr};
    QLineEdit* m_lat_edit{nullptr};
    QLineEdit* m_lon_edit{nullptr};
    QLineEdit* m_amount_edit{nullptr};
    QTableWidget* m_table{nullptr};
    QLabel* m_status_label{nullptr};
    QPushButton* m_create_button{nullptr};
    QPushButton* m_refresh_button{nullptr};
    QPushButton* m_transfer_button{nullptr};

    QString walletURI() const;
    QStringList receiveAddresses() const;
    UniValue callRpc(const std::string& method, UniValue params) const;
    void populateTable(const UniValue& data);
    QString currentPointTxid() const;
    void showError(const QString& message);
};

#endif // ORIN_QT_MAPPOINTSWIDGET_H
