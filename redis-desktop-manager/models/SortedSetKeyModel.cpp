#include "SortedSetKeyModel.h"

SortedSetKeyModel::SortedSetKeyModel(RedisClient::Connection * db, const QString &keyName, int dbIndex)
    : PaginatedModel(db, keyName, dbIndex)
{
    setColumnCount(2);
}

void SortedSetKeyModel::loadValue()
{
    QStringList command;
    
    command << "ZRANGE" << keyName << "0" << "-1" << "WITHSCORES";

    db->runCommand(RedisClient::Command(command, this, "loadedValue", dbIndex));
}

void SortedSetKeyModel::setCurrentPage(int page)
{
    if (page == currentPage) {
        return;
    }

    clear();

    QStringList labels;
    labels << "Value" << "Score";
    setHorizontalHeaderLabels(labels);

    currentPage = page;

    int size = rawData->size();

    setRowCount( (itemsOnPageLimit  > size / 2)? size / 2 : itemsOnPageLimit);

    int startShiftPosition = itemsOnPageLimit * 2 * (currentPage - 1);
    int limit = startShiftPosition + itemsOnPageLimit * 2;

    for (int i = startShiftPosition, row = 0; i < limit && i < size; ++i, ++row) {

        QStandardItem * key = new QStandardItem(rawData->at(i));
        key->setData(QVariant("member"), KeyModel::KEY_VALUE_TYPE_ROLE);

        QStandardItem * value = new QStandardItem(rawData->at(++i));
        value->setData(QVariant("score"), KeyModel::KEY_VALUE_TYPE_ROLE);

        setItem(row, 0, key);
        setItem(row, 1, value);
    }
}

int SortedSetKeyModel::itemsCount()
{
    return rawData->size() / 2;
}

void SortedSetKeyModel::updateValue(const QString& value, const QModelIndex *cellIndex)
{
    QStandardItem * currentItem = itemFromIndex(*cellIndex);    

    QString itemType = currentItem->data(KeyModel::KEY_VALUE_TYPE_ROLE).toString();

    if (itemType == "member") 
    {
        QStringList removeCmd;
        removeCmd << "ZREM"
            << keyName
            << currentItem->text();        

        db->runCommand(RedisClient::Command(removeCmd, this, dbIndex));

        QStandardItem * scoreItem = item(currentItem->row(), 1);

        QStringList addCmd;

        addCmd << "ZADD"
            << keyName
            << scoreItem->text()
            << value;

        db->runCommand(RedisClient::Command(addCmd, this, "loadedUpdateStatus", dbIndex));

    } else if (itemType == "score") {

        bool converted = false;
        double changedScore = value.toDouble(&converted);

        if (!converted) 
            return;
        
        double currentScore = currentItem->text().toDouble();
        double incr = changedScore - currentScore;

        QStandardItem * memberItem = item(currentItem->row(), 0);

        QStringList updateCmd;
        updateCmd << "ZINCRBY"
            << keyName
            << QString::number(incr)
            << memberItem->text();        

        db->runCommand(RedisClient::Command(updateCmd, this, "loadedUpdateStatus", dbIndex));
    }

    currentItem->setText(value);
}

void SortedSetKeyModel::loadedUpdateStatus(RedisClient::Response result)
{
    if (result.isErrorMessage()) 
    {
        emit valueUpdateError(result.getValue().toString());
    }
    else 
    {
        emit valueUpdated();    
    }
}
