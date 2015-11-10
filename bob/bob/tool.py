'''
bob cli tool

usage:
  bob repo list [--all] [<name>]
  bob repo add github <name> <secret>
  bob repo notify <name> <buffer>

options:
  -q, --quiet   don't say anything
'''

import logging

L = logging.getLogger('tool')

import docopt as DO
import sqlalchemy.orm as ORM

from config import conf

import common
import db
import logic
import notify

QUIET = False

def out(s, *a):
    if not QUIET:
        print(s.format(*a))

def url(*a):
    return 'http://{}/{}'.format(
        conf.web.server.name,
        '/'.join(str(x) for x in a)
        )

def find_repository(name):
    try:
        return db.session.query(db.Repository)\
            .filter(db.Repository.name == name)\
            .one()
    except ORM.exc.NoResultFound:
        out('no such repository with name {}', name)
        return None
    except ORM.exc.MultipleResultsFound:
        out('multiple repositories with name {}', name)
        return None

def c_repo(args):
    if args['list']:
        if args['<name>']:
            repo = find_repository(args['<name>'])
            if repo is None:
                return
            repos = [repo]
        else:
            repos = db.session.query(db.Repository).all()
        count = 0
        for repo in repos:
            count += 1
            out('{} ({}):', repo.name, repo.full_name)
            if args['--all']:
                out('  secret: {}', repo.secret)
            out('  notifies: {}', repo.notify)
        if count == 0:
            out('no repositories yet')

    elif args['add'] and args['github']:
        name = args['<name>'].split('/')[-1]
        full_name = args['<name>']
        secret = args['<secret>']
        r = logic.create_repository(name, full_name, secret)
        db.session.add(r)
        db.session.commit()
        out('successfully added repository: {}', url('r', r.name))

    elif args['notify']:
        repo = find_repository(args['<name>'])
        if repo is None:
            return
        repo.notify = args['<buffer>']
        db.session.commit()
        out('set the notify buffer for {} to {}', repo.name, repo.notify)

def main(args):
    global QUIET
    if '--quiet' in args and args['--quiet']:
        QUIET = True

    if args['repo']:
        c_repo(args)

if __name__ == '__main__':
    common.initialize('tool')
    main(DO.docopt(__doc__, version='bob cli 0.1'))
