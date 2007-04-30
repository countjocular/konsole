/*
    This source file is part of Konsole, a terminal emulator.

    Copyright (C) 2006 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Qt
#include <QFileInfo>
#include <QList>
#include <QString>

// KDE
#include <klocale.h>
#include <krun.h>
#include <kshell.h>
#include <kconfig.h>
#include <kstandarddirs.h>
#include <kdesktopfile.h>

// Konsole
#include "ColorScheme.h"
#include "Session.h"
#include "History.h"
#include "SessionManager.h"
#include "ShellCommand.h"

using namespace Konsole;

SessionManager* SessionManager::_instance = 0;
QHash<QString,Profile::Property> Profile::_propertyNames;

FallbackProfile::FallbackProfile()
 : Profile(0)
{
    // Fallback settings
    setProperty(Name,i18n("Shell"));
    setProperty(Command,getenv("SHELL"));
    setProperty(Arguments,QStringList() << getenv("SHELL"));
    setProperty(Font,QFont("Monospace"));

    // Fallback should not be shown in menus
    setHidden(true);
}

Profile::Profile(Profile* parent)
    : _parent(parent)
     ,_hidden(false)
{
}

bool Profile::isHidden() const { return _hidden; }
void Profile::setHidden(bool hidden) { _hidden = hidden; }

void Profile::setParent(Profile* parent) { _parent = parent; }
const Profile* Profile::parent() const { return _parent; }

bool Profile::isEmpty() const
{
    return _propertyValues.isEmpty();
}
QHash<Profile::Property,QVariant> Profile::setProperties() const
{
    return _propertyValues;
}

QVariant Profile::property(Property property) const
{
    if ( _propertyValues.contains(property) )
        return _propertyValues[property];
    else if ( _parent )
        return _parent->property(property);
    else
        return QVariant();
}
void Profile::setProperty(Property property , const QVariant& value)
{
    _propertyValues.insert(property,value);
}
bool Profile::isPropertySet(Property property) const
{
    return _propertyValues.contains(property);
}

bool Profile::isNameRegistered(const QString& name) 
{
    return _propertyNames.contains(name);
}

Profile::Property Profile::lookupByName(const QString& name)
{
    return _propertyNames[name];
}

QList<QString> Profile::namesForProperty(Property property)
{
    return _propertyNames.keys(property);
}
void Profile::registerName(Property property , const QString& name)
{
    _propertyNames.insert(name,property);
}

QString KDE4ProfileWriter::getPath(const Profile* info)
{
    QString newPath;
    
    if ( info->isPropertySet(Profile::Path) )
        newPath=info->path();

    // if the path is not specified, use the profile name + ".profile"
    if ( newPath.isEmpty() )
        newPath = info->name() + ".profile";    

    QFileInfo fileInfo(newPath);
    if (!fileInfo.isAbsolute())
        newPath = KGlobal::dirs()->saveLocation("data","konsole/") + newPath;

    qDebug() << "Saving profile under name: " << newPath;

    return newPath;
}

bool KDE4ProfileWriter::writeProfile(const QString& path , const Profile* profile)
{
    KConfig config(path,KConfig::NoGlobals);

    KConfigGroup general = config.group("General");

    if ( profile->isPropertySet(Profile::Name) )
        general.writeEntry("Name",profile->name());

    if (    profile->isPropertySet(Profile::Command) 
         || profile->isPropertySet(Profile::Arguments) )
        general.writeEntry("Command",
                ShellCommand(profile->command(),profile->arguments()).fullCommand());

    if ( profile->isPropertySet(Profile::Icon) )
        general.writeEntry("Icon",profile->icon());

    if ( profile->isPropertySet(Profile::LocalTabTitleFormat) )
        general.writeEntry("LocalTabTitleFormat",
                profile->property(Profile::LocalTabTitleFormat).value<QString>());

    if ( profile->isPropertySet(Profile::RemoteTabTitleFormat) )
        general.writeEntry("RemoteTabTitleFormat",
                profile->property(Profile::RemoteTabTitleFormat).value<QString>());

    KConfigGroup appearence = config.group("Appearence");

    if ( profile->isPropertySet(Profile::ColorScheme) )
        appearence.writeEntry("ColorScheme",profile->colorScheme());

    if ( profile->isPropertySet(Profile::Font) )
        appearence.writeEntry("Font",profile->font());
    

    return true;
}

QStringList KDE4ProfileReader::findProfiles()
{
    return KGlobal::dirs()->findAllResources("data","konsole/*.profile",
            KStandardDirs::NoDuplicates);
}
bool KDE4ProfileReader::readProfile(const QString& path , Profile* profile)
{
    qDebug() << "KDE 4 Profile Reader:" << path;

    KConfig config(path,KConfig::NoGlobals);

    KConfigGroup general = config.group("General");

    if ( general.hasKey("Name") )
        profile->setProperty(Profile::Name,general.readEntry("Name"));
    else
        return false;

    if ( general.hasKey("Command") )
    {
        ShellCommand shellCommand(general.readEntry("Command"));

        profile->setProperty(Profile::Command,shellCommand.command());
        profile->setProperty(Profile::Arguments,shellCommand.arguments());
    }

    if ( general.hasKey("Icon") )
        profile->setProperty(Profile::Icon,general.readEntry("Icon"));
    if ( general.hasKey("LocalTabTitleFormat") )
        profile->setProperty(Profile::LocalTabTitleFormat,general.readEntry("LocalTabTitleFormat"));
    if ( general.hasKey("RemoteTabTitleFormat") )
        profile->setProperty(Profile::RemoteTabTitleFormat,
                            general.readEntry("RemoteTabTitleFormat"));

    qDebug() << "local tabs:" << general.readEntry("LocalTabTitleFormat");

    KConfigGroup appearence = config.group("Appearence");

    if ( appearence.hasKey("ColorScheme") )
        profile->setProperty(Profile::ColorScheme,appearence.readEntry("Color Scheme"));
    if ( appearence.hasKey("Font") )
        profile->setProperty(Profile::Font,appearence.readEntry("Font"));

    return true;
}
QStringList KDE3ProfileReader::findProfiles()
{
    return KGlobal::dirs()->findAllResources("data", "konsole/*.desktop", 
            KStandardDirs::NoDuplicates);
}
bool KDE3ProfileReader::readProfile(const QString& path , Profile* profile)
{
    if (!QFile::exists(path))
        return false;

    KDesktopFile* desktopFile = new KDesktopFile(path);
    KConfigGroup* config = new KConfigGroup( desktopFile->desktopGroup() );

    if ( config->hasKey("Name") )
        profile->setProperty(Profile::Name,config->readEntry("Name"));

    qDebug() << "reading KDE 3 profile " << profile->name();

    if ( config->hasKey("Icon") )
        profile->setProperty(Profile::Icon,config->readEntry("Icon"));
    if ( config->hasKey("Exec") )
    {
        const QString& fullCommand = config->readEntry("Exec");
        ShellCommand shellCommand(fullCommand);
        qDebug() << "full command = " << fullCommand;
    
        profile->setProperty(Profile::Command,shellCommand.command());
        profile->setProperty(Profile::Arguments,shellCommand.arguments());

        qDebug() << "command = " << profile->command();
        qDebug() << "argumetns = " << profile->arguments();
    }
    if ( config->hasKey("Schema") )
    {
        profile->setProperty(Profile::ColorScheme,config->readEntry("Schema").replace
                                            (".schema",QString::null));        
    }
    if ( config->hasKey("defaultfont") )
    {
        profile->setProperty(Profile::Font,config->readEntry("defaultfont"));
    }
    if ( config->hasKey("KeyTab") )
    {
        profile->setProperty(Profile::KeyBindings,config->readEntry("KeyTab"));
    }
    if ( config->hasKey("Term") )
    {
        profile->setProperty(Profile::Environment,
                QStringList() << "TERM="+config->readEntry("Term"));
    }
    if ( config->hasKey("Cwd") )
    {
        profile->setProperty(Profile::Directory,config->readEntry("Cwd"));
    }

    delete desktopFile;
    delete config;

    return true;
}

#if 0

bool Profile::isAvailable() const
{
    //TODO:  Is it necessary to cache the result of the search?

    QString binary = KRun::binaryName( command(true) , false );
    binary = KShell::tildeExpand(binary);

    QString fullBinaryPath = KGlobal::dirs()->findExe(binary);

    if ( fullBinaryPath.isEmpty() )
        return false;
    else
        return true;
}
#endif

SessionManager::SessionManager()
    : _loadedAllProfiles(false)
{
    //load fallback profile
//    addProfile( new FallbackProfile );
    addProfile( new FallbackProfile );

    //locate and load default profile
    KSharedConfigPtr appConfig = KGlobal::config();
    const KConfigGroup group = appConfig->group( "Desktop Entry" );
    QString defaultSessionFilename = group.readEntry("DefaultProfile","Shell.profile");

    QString path = KGlobal::dirs()->findResource("data","konsole/"+defaultSessionFilename);
    if (!path.isEmpty())
    {
        const QString& key = loadProfile(path);
        if ( !key.isEmpty() )
            _defaultProfile = key;
    }

    Q_ASSERT( _types.count() > 0 );
    Q_ASSERT( !_defaultProfile.isEmpty() );

    // now that the session types have been loaded,
    // get the list of favorite sessions
    //loadFavorites();
}
QString SessionManager::loadProfile(const QString& path)
{
    // check that we have not already loaded this profile
    QHashIterator<QString,Profile*> iter(_types);
    while ( iter.hasNext() )
    {
        iter.next();
        if ( iter.value()->path() == path )
            return QString::null;
    }

    // load the profile
    ProfileReader* reader = 0;
    if ( path.endsWith(".desktop") )
        reader = 0; //new KDE3ProfileReader;
    else
        reader = new KDE4ProfileReader;

    if (!reader)
        return QString::null;

    Profile* newProfile = new Profile(defaultProfile());
    newProfile->setProperty(Profile::Path,path);

    bool result = reader->readProfile(path,newProfile);

    delete reader;

    if (!result)
    {
        delete newProfile;
        return QString::null;
    }
    else
        return addProfile(newProfile);
}
void SessionManager::loadAllProfiles()
{
    if ( _loadedAllProfiles )
        return;

    qDebug() << "Loading all profiles";

    KDE3ProfileReader kde3Reader;
    KDE4ProfileReader kde4Reader;

    QStringList profiles;
    profiles += kde3Reader.findProfiles();
    profiles += kde4Reader.findProfiles();
    
    QListIterator<QString> iter(profiles);
    while (iter.hasNext())
        loadProfile(iter.next());

    _loadedAllProfiles = true;
}
SessionManager::~SessionManager()
{
    QListIterator<Profile*> infoIter(_types.values());

    while (infoIter.hasNext())
        delete infoIter.next();

    // failure to call KGlobal::config()->sync() here results in a crash on exit and
    // configuraton information not being saved to disk.
    // my understanding of the documentation is that KConfig is supposed to save
    // the data automatically when the application exits.  need to discuss this
    // with people who understand KConfig better.
    qWarning() << "Manually syncing configuration information - this should be done automatically.";
    KGlobal::config()->sync();
}

const QList<Session*> SessionManager::sessions()
{
    return _sessions;
}

Session* SessionManager::createSession(QString key )
{
    Session* session = 0;
    
    const Profile* info = 0;

    if ( key.isEmpty() )
        info = defaultProfile();
    else
        info = _types[key];

    //configuration information found, create a new session based on this
    session = new Session();
    session->setType(key);
    
    applyProfile(session,info,false);

    //ask for notification when session dies
    connect( session , SIGNAL(done(Session*)) , SLOT(sessionTerminated(Session*)) );

    //add session to active list
    _sessions << session;

    Q_ASSERT( session );

    return session;
}

void SessionManager::sessionTerminated(Session* session)
{
    _sessions.removeAll(session);
    session->deleteLater();
}

QList<QString> SessionManager::availableProfiles() const
{
    return _types.keys();
}

Profile* SessionManager::profile(const QString& key) const
{
    if ( key.isEmpty() )
        return defaultProfile();

    if ( _types.contains(key) )
        return _types[key];
    else
        return 0;
}

Profile* SessionManager::defaultProfile() const
{
    return _types[_defaultProfile];
}

QString SessionManager::defaultProfileKey() const
{
    return _defaultProfile;
}

void SessionManager::saveProfile(const QString& path , Profile* info)
{
    ProfileWriter* writer = new KDE4ProfileWriter;

    QString newPath = path;

    if ( newPath.isEmpty() )
        newPath = writer->getPath(info);

    writer->writeProfile(newPath,info);

    delete writer;
}

void SessionManager::changeProfile(const QString& key , 
                                   QHash<Profile::Property,QVariant> propertyMap)
{
    Profile* info = profile(key);

    if (!info || key.isEmpty())
    {
        qWarning() << "Profile for key" << key << "not found.";
        return;
    }

    qDebug() << "Profile about to change: " << info->name();
    
    // insert the changes into the existing Profile instance
    QListIterator<Profile::Property> iter(propertyMap.keys());
    while ( iter.hasNext() )
    {
        const Profile::Property property = iter.next();
        info->setProperty(property,propertyMap[property]);
    }
    
    qDebug() << "Profile changed: " << info->name();

    // apply the changes to existing sessions
    applyProfile(key,true);

    // notify the world about the change
    emit profileChanged(key);

    // save the changes to disk
    // the path may be empty here, in which case it is up
    // to the profile writer to generate or request a path name
    if ( info->isPropertySet(Profile::Path) )
    {
        qDebug() << "Profile saved to existing path: " << info->path();
        saveProfile(info->path(),info);
    }
    else
    {
        qDebug() << "Profile saved to new path.";
        saveProfile(QString::null,info);
    }
}
void SessionManager::applyProfile(const QString& key , bool modifiedPropertiesOnly)
{
    Profile* info = profile(key);

    QListIterator<Session*> iter(_sessions);
    while ( iter.hasNext() )
    {
        Session* next = iter.next();
        if ( next->type() == key )
            applyProfile(next,info,modifiedPropertiesOnly);        
    }
}
void SessionManager::applyProfile(Session* session, const Profile* info , bool modifiedPropertiesOnly)
{
    session->setType( _types.key((Profile*)info) );

    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::Command) )
        session->setProgram(info->command());

    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::Arguments) )
        session->setArguments(info->arguments());

    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::Directory) )
        session->setInitialWorkingDirectory(info->defaultWorkingDirectory());

    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::Icon) )
        session->setIconName(info->icon());

    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::KeyBindings) )
        session->setKeymap(info->property(Profile::KeyBindings).value<QString>());

    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::LocalTabTitleFormat) )
        session->setTabTitleFormat( Session::LocalTabTitle ,
                                    info->property(Profile::LocalTabTitleFormat).value<QString>());
    if ( !modifiedPropertiesOnly || info->isPropertySet(Profile::RemoteTabTitleFormat) )
        session->setTabTitleFormat( Session::RemoteTabTitle ,
                                    info->property(Profile::RemoteTabTitleFormat).value<QString>());
}

QString SessionManager::addProfile(Profile* type)
{
    QString key;

    for ( int counter = 0;;counter++ )
    {
        if ( !_types.contains(type->path() + QString::number(counter)) )
        {
            key = type->path() + QString::number(counter);
            break;
        }
    }
    
    if ( _types.isEmpty() )
        _defaultProfile = key;

    _types.insert(key,type);

    emit profileAdded(key);

    return key;
}

void SessionManager::deleteProfile(const QString& key)
{
    Profile* type = profile(key);

    setFavorite(key,false);

    bool wasDefault = ( type == defaultProfile() );

    if ( type )
    {
        // try to delete the config file
        if ( type->isPropertySet(Profile::Path) && QFile::exists(type->path()) )
        {
            if (!QFile::remove(type->path()))
            {
                qWarning() << "Could not delete config file: " << type->path()
                    << "The file is most likely in a directory which is read-only.";
                qWarning() << "TODO: Hide this file instead.";
            }
        }

        _types.remove(key);
        delete type;
    }

    // if we just deleted the default session type,
    // replace it with the first type in the list
    if ( wasDefault )
    {
        setDefaultProfile( _types.keys().first() );
    }

    emit profileRemoved(key); 
}
void SessionManager::setDefaultProfile(const QString& key)
{
   Q_ASSERT ( _types.contains(key) );

   _defaultProfile = key;

   Profile* info = profile(key);
  
   Q_ASSERT( QFile::exists(info->path()) );
   QFileInfo fileInfo(info->path());

   qDebug() << "setting default session type to " << fileInfo.fileName();

   KConfigGroup group = KGlobal::config()->group("Desktop Entry");
   group.writeEntry("DefaultProfile",fileInfo.fileName());
}
QSet<QString> SessionManager::findFavorites() 
{
    if (_favorites.isEmpty())
        loadFavorites();

    return _favorites;
}
void SessionManager::setFavorite(const QString& key , bool favorite)
{
    Q_ASSERT( _types.contains(key) );

    if ( favorite && !_favorites.contains(key) )
    {
        qDebug() << "adding favorite - " << key;

        _favorites.insert(key);
        emit favoriteStatusChanged(key,favorite);
    
        saveFavorites();
    }
    else if ( !favorite && _favorites.contains(key) )
    {
        qDebug() << "removing favorite - " << key;
        _favorites.remove(key);
        emit favoriteStatusChanged(key,favorite);
    
        saveFavorites();
    }

}
void SessionManager::loadFavorites()
{
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup favoriteGroup = appConfig->group("Favorite Profiles");

    qDebug() << "loading favorites";

    if ( favoriteGroup.hasKey("Favorites") )
    {
        qDebug() << "found favorites key";
       QStringList list = favoriteGroup.readEntry("Favorites",QStringList());

       qDebug() << "found " << list.count() << "entries";

        QSet<QString> favoriteSet = QSet<QString>::fromList(list);

       // look for favorites amongst those already loaded
       QHashIterator<QString,Profile*> iter(_types);
       while ( iter.hasNext() )
       {
            iter.next();
            const QString& path = iter.value()->path();
            if ( favoriteSet.contains( path ) )
            {
                _favorites.insert( iter.key() );
                favoriteSet.remove(path);
            }
       }
      // load any remaining favorites
      QSetIterator<QString> unloadedFavoriteIter(favoriteSet);
      while ( unloadedFavoriteIter.hasNext() )
      {
            const QString& key = loadProfile(unloadedFavoriteIter.next());
            if (!key.isEmpty())
                _favorites.insert(key);
      } 
    }
}
void SessionManager::saveFavorites()
{
    KSharedConfigPtr appConfig = KGlobal::config();
    KConfigGroup favoriteGroup = appConfig->group("Favorite Profiles");

    QStringList paths;
    QSetIterator<QString> keyIter(_favorites);
    while ( keyIter.hasNext() )
    {
        const QString& key = keyIter.next();

        Q_ASSERT( _types.contains(key) && profile(key) != 0 );

        paths << profile(key)->path();
    }

    favoriteGroup.writeEntry("Favorites",paths);
}
void SessionManager::setInstance(SessionManager* instance)
{
    _instance = instance;
}
SessionManager* SessionManager::instance()
{
    return _instance;
}

#include "SessionManager.moc"
