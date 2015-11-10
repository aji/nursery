import logging

L = logging.getLogger('db')

import base64
import datetime as DT
import hashlib
import sqlalchemy as SA
import sqlalchemy.ext.declarative as SAD
import sqlalchemy.orm as ORM

from sqlalchemy import Column
from sqlalchemy import DateTime
from sqlalchemy import Enum
from sqlalchemy import ForeignKey
from sqlalchemy import Integer
from sqlalchemy import Sequence
from sqlalchemy import String

from config import conf

_Base = SAD.declarative_base()

class Repository(_Base):
    __tablename__ = 'repositories'

    name          = Column(String, primary_key=True)
    full_name     = Column(String)
    secret        = Column(String)
    notify        = Column(String)

class ShortURL(_Base):
    __tablename__ = 'shorturl'

    identifier    = Column(String(6), primary_key=True)
    mapping       = Column(String)
    added_at      = Column(DateTime)

def shorten(mapping):
    m = hashlib.md5()
    m.update(str(conf.db.shorten.secret))
    m.update(mapping)
    identifier = base64.urlsafe_b64encode(m.digest())[:6]
    try:
        shorturl = session.query(ShortURL)\
            .filter(ShortURL.identifier == identifier)\
            .one()
        shorturl.mapping = mapping
        shorturl.added_at = now()
    except ORM.exc.NoResultFound:
        shorturl = ShortURL(
            identifier = identifier,
            mapping = mapping,
            added_at = now(),
            )
        session.add(shorturl)
    return shorturl.identifier

def unshorten(identifier):
    try:
        shorturl = session.query(ShortURL)\
            .filter(ShortURL.identifier == identifier)\
            .one()
    except ORM.exc.NoResultFound:
        return None
    return shorturl.mapping

def now():
    return DT.datetime.now()

def dispose():
    _engine.dispose()

def initialize():
    global _engine, session

    L.info('sqlalchemy version: %s', SA.__version__)

    _engine = SA.create_engine(str(conf.db.uri), echo=bool(int(conf.db.echo)))
    Session = ORM.sessionmaker(bind=_engine)

    session = Session()

    _Base.metadata.create_all(_engine)
