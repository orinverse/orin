// Copyright (c) 2025 The Orin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/mappointswidget.h>

#include <qt/addresstablemodel.h>
#include <qt/guiutil.h>
#include <qt/walletmodel.h>

#include <interfaces/node.h>

#include <QComboBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {
UniValue MakeNumeric(const QString& value)
{
    UniValue v;
    v.setNumStr(value.toStdString());
    return v;
}
} // namespace

MapPointsWidget::MapPointsWidget(QWidget* parent) : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    auto intro = new QLabel(tr("Create and manage RealMap points stored on-chain."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    QFormLayout* form = new QFormLayout();

    m_address_combo = new QComboBox(this);
    m_address_combo->setEditable(false);
    form->addRow(tr("Owner address"), m_address_combo);

    auto latValidator = new QDoubleValidator(-90.0, 90.0, 6, this);
    latValidator->setNotation(QDoubleValidator::StandardNotation);
    m_lat_edit = new QLineEdit(this);
    m_lat_edit->setPlaceholderText(tr("Latitude (e.g. 55.751244)"));
    m_lat_edit->setValidator(latValidator);
    form->addRow(tr("Latitude"), m_lat_edit);

    auto lonValidator = new QDoubleValidator(-180.0, 180.0, 6, this);
    lonValidator->setNotation(QDoubleValidator::StandardNotation);
    m_lon_edit = new QLineEdit(this);
    m_lon_edit->setPlaceholderText(tr("Longitude (e.g. 37.618423)"));
    m_lon_edit->setValidator(lonValidator);
    form->addRow(tr("Longitude"), m_lon_edit);

    auto amountValidator = new QDoubleValidator(0.00000001, 21000000.0, 8, this);
    amountValidator->setNotation(QDoubleValidator::StandardNotation);
    m_amount_edit = new QLineEdit(this);
    m_amount_edit->setValidator(amountValidator);
    m_amount_edit->setText(QStringLiteral("0.01"));
    form->addRow(tr("Amount (ORIN)"), m_amount_edit);

    m_create_button = new QPushButton(tr("Create point"), this);
    m_refresh_button = new QPushButton(tr("Refresh"), this);
    m_transfer_button = new QPushButton(tr("Transfer ownership"), this);
    connect(m_create_button, &QPushButton::clicked, this, &MapPointsWidget::handleCreatePoint);
    connect(m_refresh_button, &QPushButton::clicked, this, &MapPointsWidget::refreshPoints);
    connect(m_transfer_button, &QPushButton::clicked, this, &MapPointsWidget::handleTransferPoint);

    QHBoxLayout* button_row = new QHBoxLayout();
    button_row->addWidget(m_create_button);
    button_row->addWidget(m_refresh_button);
    button_row->addWidget(m_transfer_button);
    button_row->addStretch();

    layout->addLayout(form);
    layout->addLayout(button_row);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("Point ID"), tr("Current owner"), tr("Latitude"), tr("Longitude"), tr("Height")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &MapPointsWidget::updateButtons);
    layout->addWidget(m_table);

    m_status_label = new QLabel(tr("No map points to display."), this);
    layout->addWidget(m_status_label);

    m_create_button->setEnabled(false);
    m_refresh_button->setEnabled(false);
    m_transfer_button->setEnabled(false);
}

void MapPointsWidget::setWalletModel(WalletModel* model)
{
    m_wallet_model = model;
    m_address_model = model ? model->getAddressTableModel() : nullptr;
    const bool enabled = model != nullptr;
    m_create_button->setEnabled(enabled);
    m_refresh_button->setEnabled(enabled);

    if (m_address_model) {
        connect(m_address_model, &QAbstractItemModel::modelReset, this, &MapPointsWidget::updateAddressList);
        connect(m_address_model, &QAbstractItemModel::rowsInserted, this, &MapPointsWidget::updateAddressList);
        connect(m_address_model, &QAbstractItemModel::rowsRemoved, this, &MapPointsWidget::updateAddressList);
        connect(m_address_model, &QAbstractItemModel::dataChanged, this, &MapPointsWidget::updateAddressList);
    }
    updateAddressList();
    updateButtons();
}

QString MapPointsWidget::walletURI() const
{
    if (!m_wallet_model) return QString();
    const QString name = m_wallet_model->getWalletName();
    const QByteArray encoded = QUrl::toPercentEncoding(name);
    return QStringLiteral("/wallet/%1").arg(QString::fromUtf8(encoded));
}

QStringList MapPointsWidget::receiveAddresses() const
{
    QStringList list;
    if (!m_address_model) return list;

    const int rows = m_address_model->rowCount(QModelIndex());
    for (int row = 0; row < rows; ++row) {
        QModelIndex index = m_address_model->index(row, AddressTableModel::Address, QModelIndex());
        const QString type = m_address_model->data(index, AddressTableModel::TypeRole).toString();
        if (type == AddressTableModel::Receive) {
            list << m_address_model->data(index, Qt::DisplayRole).toString();
        }
    }
    return list;
}

UniValue MapPointsWidget::callRpc(const std::string& method, UniValue params) const
{
    if (!m_wallet_model) {
        throw std::runtime_error("Wallet not loaded");
    }
    const std::string uri = walletURI().toStdString();
    return m_wallet_model->node().executeRpc(method, params, uri);
}

void MapPointsWidget::populateTable(const UniValue& data)
{
    m_table->setRowCount(0);
    if (!data.isArray()) {
        m_status_label->setText(tr("Unexpected RPC reply."));
        return;
    }
    const size_t count = data.size();
    m_table->setRowCount(static_cast<int>(count));
    auto stringField = [](const UniValue& entry, const char* key) -> QString {
        const UniValue& value = entry.find_value(key);
        return value.isStr() ? QString::fromStdString(value.get_str()) : QString();
    };
    auto doubleField = [](const UniValue& entry, const char* key) -> double {
        const UniValue& value = entry.find_value(key);
        return value.isNum() ? value.get_real() : 0.0;
    };
    auto intField = [](const UniValue& entry, const char* key) -> int {
        const UniValue& value = entry.find_value(key);
        return value.isNum() ? value.getInt<int>() : 0;
    };
    for (size_t i = 0; i < count; ++i) {
        const UniValue& entry = data[i];
        const QString owner = stringField(entry, "current_owner");
        const QString txid = stringField(entry, "origin_txid");
        const double lat = doubleField(entry, "lat");
        const double lon = doubleField(entry, "lon");
        const int height = intField(entry, "origin_height");

        auto lat_item = new QTableWidgetItem(QString::number(lat, 'f', 6));
        auto lon_item = new QTableWidgetItem(QString::number(lon, 'f', 6));
        auto height_item = new QTableWidgetItem(QString::number(height));
        auto tx_item = new QTableWidgetItem(txid);
        auto owner_item = new QTableWidgetItem(owner);

        owner_item->setFlags(owner_item->flags() ^ Qt::ItemIsEditable);
        lat_item->setFlags(lat_item->flags() ^ Qt::ItemIsEditable);
        lon_item->setFlags(lon_item->flags() ^ Qt::ItemIsEditable);
        height_item->setFlags(height_item->flags() ^ Qt::ItemIsEditable);
        tx_item->setFlags(tx_item->flags() ^ Qt::ItemIsEditable);

        m_table->setItem(i, 0, tx_item);
        m_table->setItem(i, 1, owner_item);
        m_table->setItem(i, 2, lat_item);
        m_table->setItem(i, 3, lon_item);
        m_table->setItem(i, 4, height_item);
    }
    if (count == 0) {
        m_status_label->setText(tr("No map points associated with wallet addresses."));
    } else {
        m_status_label->setText(tr("Showing %1 map point(s).").arg(static_cast<int>(count)));
    }
}

void MapPointsWidget::handleCreatePoint()
{
    if (!m_wallet_model) {
        return;
    }
    const QString owner = m_address_combo->currentData().toString();
    if (owner.isEmpty()) {
        showError(tr("Select an owner address before creating a point."));
        return;
    }

    bool ok_lat{false};
    bool ok_lon{false};
    bool ok_amount{false};
    const double lat = m_lat_edit->text().toDouble(&ok_lat);
    const double lon = m_lon_edit->text().toDouble(&ok_lon);
    const double amount = m_amount_edit->text().toDouble(&ok_amount);
    if (!ok_lat || !ok_lon) {
        showError(tr("Latitude and longitude must be valid decimal numbers."));
        return;
    }
    if (!ok_amount || amount <= 0.0) {
        showError(tr("Amount must be greater than zero."));
        return;
    }

    UniValue params(UniValue::VARR);
    params.push_back(owner.toStdString());
    params.push_back(MakeNumeric(QString::number(lat, 'f', 6)));
    params.push_back(MakeNumeric(QString::number(lon, 'f', 6)));
    params.push_back(MakeNumeric(QString::number(amount, 'f', 8)));

    try {
        UniValue result = callRpc("sendmappoint", params);
        QString txid;
        if (result.isStr()) {
            txid = QString::fromStdString(result.get_str());
        } else if (result.isObject()) {
            txid = QString::fromStdString(result.find_value("txid").get_str());
        }
        QMessageBox::information(this, tr("Point created"), tr("Transaction id: %1").arg(txid));
        refreshPoints();
    } catch (UniValue& error) {
        const QString message = QString::fromStdString(error.write());
        showError(message);
    } catch (const std::exception& e) {
        showError(QString::fromStdString(e.what()));
    }
}

void MapPointsWidget::refreshPoints()
{
    if (!m_wallet_model) {
        return;
    }
    const QStringList addresses = receiveAddresses();
    if (addresses.isEmpty()) {
        m_table->setRowCount(0);
        m_status_label->setText(tr("No receiving addresses available."));
        m_table->clearSelection();
        updateButtons();
        return;
    }
    UniValue params(UniValue::VARR);
    UniValue addr_list(UniValue::VARR);
    for (const QString& addr : addresses) {
        addr_list.push_back(addr.toStdString());
    }
    params.push_back(addr_list);
    try {
        UniValue result = callRpc("getaddresspoints", params);
        populateTable(result);
        m_table->clearSelection();
        updateButtons();
    } catch (UniValue& error) {
        showError(QString::fromStdString(error.write()));
    } catch (const std::exception& e) {
        showError(QString::fromStdString(e.what()));
    }
}

void MapPointsWidget::updateAddressList()
{
    m_address_combo->clear();
    const QStringList addresses = receiveAddresses();
    for (const QString& addr : addresses) {
        const QString label = m_address_model ? m_address_model->labelForAddress(addr) : QString();
        const QString display = label.isEmpty() ? addr : tr("%1 (%2)").arg(label, addr);
        m_address_combo->addItem(display, addr);
    }
    const bool has_addresses = !addresses.isEmpty();
    m_create_button->setEnabled(has_addresses && m_wallet_model);
    m_refresh_button->setEnabled(has_addresses && m_wallet_model);
    updateButtons();
}

QString MapPointsWidget::currentPointTxid() const
{
    if (!m_table) {
        return QString();
    }
    const QItemSelectionModel* selection = m_table->selectionModel();
    if (!selection) {
        return QString();
    }
    const QModelIndexList rows = selection->selectedRows();
    if (rows.isEmpty()) {
        return QString();
    }
    const int row = rows.first().row();
    QTableWidgetItem* item = m_table->item(row, 0);
    return item ? item->text() : QString();
}

void MapPointsWidget::handleTransferPoint()
{
    if (!m_wallet_model || !m_transfer_button->isEnabled()) {
        return;
    }
    const QString point_txid = currentPointTxid();
    if (point_txid.isEmpty()) {
        showError(tr("Select a map point to transfer ownership."));
        return;
    }

    bool ok{false};
    QString new_owner = QInputDialog::getText(this,
                                              tr("Transfer ownership"),
                                              tr("New owner address"),
                                              QLineEdit::Normal,
                                              QString(),
                                              &ok);
    new_owner = new_owner.trimmed();
    if (!ok || new_owner.isEmpty()) {
        return;
    }

    bool ok_amount{false};
    double default_amount = m_amount_edit->text().toDouble(&ok_amount);
    if (!ok_amount || default_amount <= 0.0) {
        default_amount = 0.01;
    }
    double amount = QInputDialog::getDouble(this,
                                            tr("Transfer ownership"),
                                            tr("Amount (ORIN)"),
                                            default_amount,
                                            0.00000001,
                                            21000000.0,
                                            8,
                                            &ok_amount);
    if (!ok_amount || amount <= 0.0) {
        showError(tr("Amount must be greater than zero."));
        return;
    }

    UniValue params(UniValue::VARR);
    params.push_back(point_txid.toStdString());
    params.push_back(new_owner.toStdString());
    params.push_back(MakeNumeric(QString::number(amount, 'f', 8)));

    try {
        UniValue result = callRpc("sendpointtransfer", params);
        QString txid;
        if (result.isStr()) {
            txid = QString::fromStdString(result.get_str());
        } else if (result.isObject()) {
            txid = QString::fromStdString(result.find_value("txid").get_str());
        }
        QMessageBox::information(this, tr("Ownership transferred"), tr("Transaction id: %1").arg(txid));
        refreshPoints();
    } catch (UniValue& error) {
        showError(QString::fromStdString(error.write()));
    } catch (const std::exception& e) {
        showError(QString::fromStdString(e.what()));
    }
}

void MapPointsWidget::updateButtons()
{
    if (!m_transfer_button) {
        return;
    }
    const bool has_wallet = m_wallet_model != nullptr;
    const bool has_selection = !currentPointTxid().isEmpty();
    m_transfer_button->setEnabled(has_wallet && has_selection);
}

void MapPointsWidget::showError(const QString& message)
{
    QMessageBox::critical(this, tr("Map point error"), message);
}
